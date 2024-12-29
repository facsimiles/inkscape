// SPDX-License-Identifier: GPL-2.0-or-later
/** \file
 * Pen event context implementation.
 */

/*
 * Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Jon A. Cruz <jon@joncruz.org>
 *
 * Copyright (C) 2000 Lauris Kaplinski
 * Copyright (C) 2000-2001 Ximian, Inc.
 * Copyright (C) 2002 Lauris Kaplinski
 * Copyright (C) 2004 Monash University
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "pen-tool.h"

#include <cstring>
#include <gdk/gdkkeysyms.h>
#include <glibmm/i18n.h>
#include <memory>
#include <string>

#include "context-fns.h"
#include "desktop.h"
#include "display/control/canvas-item-bpath.h"
#include "display/control/canvas-item-ctrl.h"
#include "display/control/canvas-item-curve.h"
#include "display/curve.h"
#include "freehand-base.h"
#include "message-context.h"
#include "message-stack.h"
#include "object/sp-path.h"
#include "selection-chemistry.h"
#include "selection.h"
#include "ui/draw-anchor.h"
#include "ui/tools/pen-tool.h"
#include "ui/widget/events/canvas-event.h"

// we include the necessary files for BSpline & Spiro
#include "live_effects/effect.h"    // for Effect
#include "live_effects/lpeobject.h" // for LivePathEffectObject

#define INKSCAPE_LPE_SPIRO_C
#include "live_effects/lpe-spiro.h" // for sp_spiro_do_effect

#define INKSCAPE_LPE_BSPLINE_C
#include "live_effects/lpe-bspline.h" // for sp_bspline_do_effect

// Given an optionally-present SPCurve, e.g. a smart/raw pointer or an optional,
// return a copy of its pathvector if present, or a blank pathvector otherwise.
template <typename T>
static Geom::PathVector copy_pathvector_optional(T &p)
{
    if (p) {
        return p->get_pathvector();
    } else {
        return {};
    }
}

static constexpr int NONE_SELECTED = -1;

namespace Inkscape {
namespace UI {
namespace Tools {

static Geom::Point pen_drag_origin_w(0, 0);
static bool pen_within_tolerance = false;
static CanvasItemGroup *canvas;

PenTool::PenTool(SPDesktop *desktop, std::string &&prefs_path, std::string &&cursor_filename)
    : FreehandBase(desktop, std::move(prefs_path), std::move(cursor_filename))
    , _acc_to_line{"tool.pen.to-line"}
    , _acc_to_curve{"tool.pen.to-curve"}
    , _acc_to_guides{"tool.pen.to-guides"}
{
    tablet_enabled = false;

    // Pen indicators (temporary handles shown when adding a new node).
    canvas = desktop->getCanvasControls();

    cl0 = make_canvasitem<CanvasItemCurve>(canvas);
    cl1 = make_canvasitem<CanvasItemCurve>(canvas);
    cl0->set_visible(false);
    cl1->set_visible(false);

    fh_anchor = std::make_unique<SPDrawAnchor>(this, green_curve, true, Geom::Point(0, 0));
    bh_anchor = std::make_unique<SPDrawAnchor>(this, green_curve, true, Geom::Point(0, 0));
    fh_anchor->ctrl->set_visible(false);
    bh_anchor->ctrl->set_visible(false);
    fh_anchor->ctrl->set_type(CANVAS_ITEM_CTRL_TYPE_ROTATE);
    bh_anchor->ctrl->set_type(CANVAS_ITEM_CTRL_TYPE_ROTATE);

    int sz = sizeof(ctrl) / sizeof(ctrl[0]);
    for (int i = 0; i < sz; i++) {
        ctrl[i] = make_canvasitem<CanvasItemCtrl>(canvas, ctrl_types[i]);
        ctrl[i]->set_visible(false);
    }

    sp_event_context_read(this, "mode");

    this->anchor_statusbar = false;

    this->setPolylineMode();

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    if (prefs->getBool("/tools/freehand/pen/selcue")) {
        this->enableSelectionCue();
    }

    _desktop_destroy = _desktop->connectDestroy([this](SPDesktop *) { state = State::DEAD; });
}

PenTool::~PenTool()
{
    _desktop_destroy.disconnect();
    this->discard_delayed_snap_event();

    if (this->npoints != 0) {
        // switching context - finish path
        this->ea = nullptr; // unset end anchor if set (otherwise crashes)
        if (state != State::DEAD) {
            _finish(false);
        }
    }

    for (auto &c : ctrl) {
        c.reset();
    }
    cl0.reset();
    cl1.reset();

    // remove all anchors
    anchors.clear();
    node_index = NONE_SELECTED;

    fh_anchor.reset();
    bh_anchor.reset();
    selected_anchor = nullptr;

    if (this->waiting_item && this->expecting_clicks_for_LPE > 0) {
        // we received too few clicks to sanely set the parameter path so we remove the LPE from the item
        this->waiting_item->removeCurrentPathEffect(false);
    }
}

void PenTool::setPolylineMode()
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    guint mode = prefs->getInt("/tools/freehand/pen/freehand-mode", 0);
    // change the nodes to make space for bspline mode
    this->is_polylines_only = (mode == 3 || mode == 4);
    this->is_polylines_paraxial = (mode == 4);
    this->is_spiro = (mode == 1);
    this->is_bspline = (mode == 2);
    this->is_bezier = !(is_polylines_only || is_polylines_paraxial || is_spiro || is_bspline);
    this->_bsplineSpiroColor();
    if (!this->green_bpaths.empty()) {
        this->_redrawAll(true);
    }
}

void PenTool::_cancel()
{
    this->state = PenTool::STOP;
    this->_resetColors();
    for (auto &c : ctrl) {
        c->set_visible(false);
    }
    cl0->set_visible(false);
    cl1->set_visible(false);

    // remove all anchors
    anchors.clear();
    node_index = NONE_SELECTED;

    fh_anchor->ctrl->set_visible(false);
    fh_anchor->ctrl->set_normal();
    fh_anchor->ctrl->set_size(Inkscape::HandleSize::NORMAL);
    fh_anchor->active = false;

    bh_anchor->ctrl->set_visible(false);
    bh_anchor->ctrl->set_normal();
    bh_anchor->ctrl->set_size(Inkscape::HandleSize::NORMAL);
    bh_anchor->active = false;

    selected_anchor = nullptr;
    drag_handle_statusbar = false;
    node_mode_statusbar = false;

    this->message_context->clear();
    this->message_context->flash(Inkscape::NORMAL_MESSAGE, _("Drawing cancelled"));
    _redo_stack.clear();
}

/**
 * Callback that sets key to value in pen context.
 */
void PenTool::set(const Inkscape::Preferences::Entry &val)
{
    Glib::ustring name = val.getEntryName();

    if (name == "mode") {
        if (val.getString() == "drag") {
            this->mode = MODE_DRAG;
        } else {
            this->mode = MODE_CLICK;
        }
    }
}

bool PenTool::hasWaitingLPE()
{
    // note: waiting_LPE_type is defined in SPDrawContext
    return (this->waiting_LPE != nullptr || this->waiting_LPE_type != Inkscape::LivePathEffect::INVALID_LPE);
}

/**
 * Snaps new node relative to the previous node.
 */
void PenTool::_endpointSnap(Geom::Point &p, guint const state)
{
    // Paraxial kicks in after first line has set the angle (before then it's a free line)
    bool poly = this->is_polylines_paraxial && !this->green_curve->is_unset();

    if ((state & GDK_CONTROL_MASK) && !poly) { // CTRL enables angular snapping
        if (this->npoints > 0) {
            spdc_endpoint_snap_rotation(this, p, p_array[0], state);
        } else {
            std::optional<Geom::Point> origin = std::optional<Geom::Point>();
            spdc_endpoint_snap_free(this, p, origin);
        }
    } else {
        // We cannot use shift here to disable snapping because the shift-key is already used
        // to toggle the paraxial direction; if the user wants to disable snapping (s)he will
        // have to use the %-key, the menu, or the snap toolbar
        if ((this->npoints > 0) && poly) {
            // snap constrained
            this->_setToNearestHorizVert(p, state);
        } else {
            // snap freely
            std::optional<Geom::Point> origin = this->npoints > 0 ? p_array[0] : std::optional<Geom::Point>();
            spdc_endpoint_snap_free(this, p,
                                    origin); // pass the origin, to allow for perpendicular / tangential snapping
        }
    }
}

/**
 * Snaps new node's handle relative to the new node.
 */
void PenTool::_endpointSnapHandle(Geom::Point &p, guint const state)
{
    g_return_if_fail((this->npoints == 2 || this->npoints == 5));

    if ((state & GDK_CONTROL_MASK)) { // CTRL enables angular snapping
        spdc_endpoint_snap_rotation(this, p, p_array[this->npoints - 2], state);
    } else {
        if (!(state & GDK_SHIFT_MASK)) { // SHIFT disables all snapping, except the angular snapping above
            std::optional<Geom::Point> origin = p_array[this->npoints - 2];
            spdc_endpoint_snap_free(this, p, origin);
        }
    }
}

bool PenTool::item_handler(SPItem *item, CanvasEvent const &event)
{
    bool ret = false;

    inspect_event(
        event, [&](ButtonPressEvent const &event) { ret = _handleButtonPress(event); },
        [&](ButtonReleaseEvent const &event) { ret = _handleButtonRelease(event); }, [&](CanvasEvent const &event) {});

    return ret || FreehandBase::item_handler(item, event);
}

/**
 * Callback to handle all pen events.
 */
bool PenTool::root_handler(CanvasEvent const &event)
{
    bool ret = false;

    inspect_event(
        event,
        [&](ButtonPressEvent const &event) {
            if (event.num_press == 1) {
                ret = _handleButtonPress(event);
            } else if (event.num_press == 2) {
                ret = _handle2ButtonPress(event);
            }
        },
        [&](MotionEvent const &event) { ret = _handleMotionNotify(event); },
        [&](ButtonReleaseEvent const &event) { ret = _handleButtonRelease(event); },
        [&](KeyPressEvent const &event) { ret = _handleKeyPress(event); }, [&](CanvasEvent const &event) {});

    return ret || FreehandBase::root_handler(event);
}

/**
 * Handle mouse single button press event.
 */
bool PenTool::_handleButtonPress(ButtonPressEvent const &event)
{
    if (events_disabled) {
        // skip event processing if events are disabled
        return false;
    }

    Geom::Point const event_w(event.pos);
    Geom::Point event_dt(_desktop->w2d(event_w));
    // Test whether we hit any anchor.
    SPDrawAnchor *const anchor = spdc_test_inside(this, event_w);

    // with this we avoid creating a new point over the existing one
    if (event.button != 3 && (is_spiro || is_bspline) && npoints > 0 && p_array[0] == p_array[3]) {
        if (anchor && anchor == sa && green_curve->is_unset()) {
            // remove the following line to avoid having one node on top of another
            _finishSegment(event_dt, event.modifiers);
            _finish(true);
            return true;
        }
        return false;
    }

    bool ret = false;

    if (event.button == 1 && expecting_clicks_for_LPE != 1) { // Make sure this is not the last click for a waiting LPE
                                                              // (otherwise we want to finish the path)

        if (Inkscape::have_viable_layer(_desktop, defaultMessageContext()) == false) {
            return true;
        }

        SPDrawAnchor *prev = nullptr;
        if (!anchors.empty() && !is_polylines_only && !is_polylines_paraxial) {
            prev = anchors.back()->anchorTest(event_w, true);
        }

        grabCanvasEvents();

        pen_drag_origin_w = event_w;
        pen_within_tolerance = true;

        switch (mode) {
            case PenTool::MODE_CLICK:
                // In click mode we add point on release
                switch (state) {
                    case PenTool::POINT:
                    case PenTool::CONTROL:
                    case PenTool::CLOSE:
                    case PenTool::BREAK:
                    case PenTool::NODE:
                    case PenTool::HANDLE:
                        break;
                    case PenTool::STOP:
                        // This is allowed, if we just canceled curve
                        state = PenTool::POINT;
                        break;
                    default:
                        break;
                }
                break;
            case PenTool::MODE_DRAG:
                switch (state) {
                    case PenTool::STOP:
                        // This is allowed, if we just canceled curve
                    case PenTool::POINT:
                        if (npoints == 0) {
                            _bsplineSpiroColor();
                            Geom::Point p;
                            if ((event.modifiers & GDK_CONTROL_MASK) && (is_polylines_only || is_polylines_paraxial)) {
                                p = event_dt;
                                if (!(event.modifiers & GDK_SHIFT_MASK)) {
                                    auto &m = _desktop->getNamedView()->snap_manager;
                                    m.setup(_desktop);
                                    m.freeSnapReturnByRef(p, Inkscape::SNAPSOURCE_NODE_HANDLE);
                                    m.unSetup();
                                }
                                spdc_create_single_dot(this, p, "/tools/freehand/pen", event.modifiers);
                                ret = true;
                                break;
                            }

                            // TODO: Perhaps it would be nicer to rearrange the following case
                            // distinction so that the case of a waiting LPE is treated separately

                            // Set start anchor
                            sa = anchor;
                            if (anchor) {
                                // Put the start overwrite curve always on the same direction
                                if (anchor->start) {
                                    sa_overwrited = std::make_shared<SPCurve>(sa->curve->reversed());
                                } else {
                                    sa_overwrited = std::make_shared<SPCurve>(*sa->curve);
                                }
                                _bsplineSpiroStartAnchor(event.modifiers & GDK_SHIFT_MASK);
                            }
                            if (anchor && (!hasWaitingLPE() || is_bspline || is_spiro)) {
                                // Adjust point to anchor if needed; if we have a waiting LPE, we need
                                // a fresh path to be created so don't continue an existing one
                                p = anchor->dp;
                                _desktop->messageStack()->flash(Inkscape::NORMAL_MESSAGE,
                                                                _("Continuing selected path"));
                            } else {
                                // This is the first click of a new curve; deselect item so that
                                // this curve is not combined with it (unless it is drawn from its
                                // anchor, which is handled by the sibling branch above)
                                Inkscape::Selection *const selection = _desktop->getSelection();
                                if (!(event.modifiers & GDK_SHIFT_MASK) || hasWaitingLPE()) {
                                    // if we have a waiting LPE, we need a fresh path to be created
                                    // so don't append to an existing one
                                    selection->clear();
                                    _desktop->messageStack()->flash(Inkscape::NORMAL_MESSAGE, _("Creating new path"));
                                } else if (selection->singleItem() && is<SPPath>(selection->singleItem())) {
                                    _desktop->messageStack()->flash(Inkscape::NORMAL_MESSAGE,
                                                                    _("Appending to selected path"));
                                }

                                // Create green anchor
                                p = event_dt;
                                _endpointSnap(p, event.modifiers);
                                green_anchor = std::make_shared<SPDrawAnchor>(this, green_curve, true, p);
                                anchors.push_back(green_anchor);
                                if (is_bspline || is_spiro)
                                    anchors.back()->ctrl->set_type(CANVAS_ITEM_CTRL_TYPE_ROTATE);
                            }
                            _setInitialPoint(p);
                        } else {
                            // Set end anchor
                            ea = anchor;
                            Geom::Point p;
                            if (anchor) {
                                p = anchor->dp;
                                // we hit an anchor, will finish the curve (either with or without closing)
                                // in release handler
                                state = PenTool::CLOSE;

                                if (green_anchor && green_anchor->active) {
                                    // we clicked on the current curve start, so close it even if
                                    // we drag a handle away from it
                                    green_closed = true;
                                }
                                ret = true;
                                break;

                            } else if (prev) {
                                // we hit the previous node anchor, we will break the front handle
                                // of the previous node
                                p = prev->dp;
                                state = PenTool::BREAK;

                                ret = true;
                                break;

                            } else {
                                p = event_dt;
                                _endpointSnap(p, event.modifiers); // Snap node only if not hitting anchor.
                                _setSubsequentPoint(p, true);
                            }
                        }
                        // avoid the creation of a control point so a node is created in the release event
                        state = (is_spiro || is_bspline || is_polylines_only) ? PenTool::POINT : PenTool::CONTROL;
                        ret = true;
                        break;
                    case PenTool::CONTROL:
                        g_warning("Button down in CONTROL state");
                        break;
                    case PenTool::CLOSE:
                        g_warning("Button down in CLOSE state");
                        break;
                    case PenTool::BREAK:
                        g_warning("Button down in BREAK state");
                        break;
                    case PenTool::NODE:

                        if (!(event.modifiers & GDK_ALT_MASK)) {
                            state = PenTool::POINT;
                            break;
                        }

                        if (node_index == NONE_SELECTED) {
                            for (int i = 0; i < anchors.size(); i++) {
                                if (anchors[i]->anchorTest(event_w, true)) {
                                    node_index = i;
                                    break;
                                }
                            }
                        }

                        break;

                    case PenTool::HANDLE:

                        if (!(event.modifiers & GDK_SHIFT_MASK)) {
                            state = PenTool::POINT;
                            break;
                        }

                        if (!selected_anchor)
                            selected_anchor = bh_anchor->anchorTest(event_w, true);
                        if (!selected_anchor)
                            selected_anchor = fh_anchor->anchorTest(event_w, true);

                        drag_handle = selected_anchor != nullptr;

                    default:
                        break;
                }
                break;
            default:
                break;
        }
    } else if (expecting_clicks_for_LPE == 1 && npoints != 0) {
        // when the last click for a waiting LPE occurs we want to finish the path
        _finishSegment(event_dt, event.modifiers);
        if (green_closed) {
            // finishing at the start anchor, close curve
            _finish(true);
        } else {
            // finishing at some other anchor, finish curve but not close
            _finish(false);
        }

        ret = true;
    } else if (event.button == 3 && npoints != 0 && !_button1on) {
        // right click - finish path, but only if the left click isn't pressed.
        ea = nullptr; // unset end anchor if set (otherwise crashes)
        _finish(false);
        ret = true;
    }

    if (expecting_clicks_for_LPE > 0) {
        --expecting_clicks_for_LPE;
    }

    return ret;
}

/**
 * Handle mouse double button press event.
 */
bool PenTool::_handle2ButtonPress(ButtonPressEvent const &event)
{
    bool ret = false;
    // Only end on LMB double click. Otherwise horizontal scrolling causes ending of the path
    if (npoints != 0 && event.button == 1 && state != PenTool::CLOSE) {
        _finish(false);
        ret = true;
    }
    return ret;
}

/**
 * Handle motion_notify event.
 */
bool PenTool::_handleMotionNotify(MotionEvent const &event)
{
    bool ret = false;

    if (event.modifiers & GDK_BUTTON2_MASK) {
        // allow scrolling
        return false;
    }

    if (events_disabled) {
        // skip motion events if pen events are disabled
        return false;
    }

    Geom::Point const event_w(event.pos);

    // we take out the function the const "tolerance" because we need it later
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    gint const tolerance = prefs->getIntLimited("/options/dragtolerance/value", 0, 0, 100);

    if (pen_within_tolerance) {
        if (Geom::LInfty(event_w - pen_drag_origin_w) < tolerance) {
            return false; // Do not drag if we're within tolerance from origin.
        }
    }
    // Once the user has moved farther than tolerance from the original location
    // (indicating they intend to move the object, not click), then always process the
    // motion notify coordinates as given (no snapping back to origin)
    pen_within_tolerance = false;

    // Find desktop coordinates
    Geom::Point p = _desktop->w2d(event_w);

    // Test, whether we hit any anchor
    SPDrawAnchor *anchor = spdc_test_inside(this, event_w);

    SPDrawAnchor *prev = nullptr;
    if (!anchors.empty() && !is_polylines_only && !is_polylines_paraxial) {
        prev = anchors.back()->anchorTest(event_w, true);
    }

    switch (mode) {
        case PenTool::MODE_CLICK:
            switch (state) {
                case PenTool::POINT:
                    if (npoints != 0) {
                        // Only set point, if we are already appending
                        _endpointSnap(p, event.modifiers);
                        _setSubsequentPoint(p, true);
                        ret = true;
                    } else if (!sp_event_context_knot_mouseover()) {
                        SnapManager &m = _desktop->getNamedView()->snap_manager;
                        m.setup(_desktop);
                        m.preSnap(Inkscape::SnapCandidatePoint(p, Inkscape::SNAPSOURCE_NODE_HANDLE));
                        m.unSetup();
                    }
                    break;
                case PenTool::CONTROL:
                case PenTool::CLOSE:
                    // Placing controls is last operation in CLOSE state
                    _endpointSnap(p, event.modifiers);
                    _setCtrl(p, event.modifiers);
                    ret = true;
                    break;
                case PenTool::BREAK:
                case PenTool::NODE:
                case PenTool::HANDLE:
                    break;
                case PenTool::STOP:
                    if (!sp_event_context_knot_mouseover()) {
                        SnapManager &m = _desktop->getNamedView()->snap_manager;
                        m.setup(_desktop);
                        m.preSnap(Inkscape::SnapCandidatePoint(p, Inkscape::SNAPSOURCE_NODE_HANDLE));
                        m.unSetup();
                    }
                    break;
                default:
                    break;
            }
            break;
        case PenTool::MODE_DRAG:
            switch (state) {
                case PenTool::POINT:
                    if (npoints > 0) {
                        // Make sure the handle anchors are inactive
                        if (fh_anchor->active) {
                            fh_anchor->ctrl->set_normal();
                            fh_anchor->ctrl->set_size(Inkscape::HandleSize::NORMAL);
                            fh_anchor->active = false;
                        }

                        if (bh_anchor->active) {
                            bh_anchor->ctrl->set_normal();
                            bh_anchor->ctrl->set_size(Inkscape::HandleSize::NORMAL);
                            bh_anchor->active = false;
                        }

                        // Only set point, if we are already appending

                        // if ALT key held switch to NODE tool
                        if ((event.modifiers & GDK_ALT_MASK) && (is_bezier || is_spiro)) {
                            state = PenTool::NODE;
                            red_curve.reset();
                            this->red_bpath->set_bpath(&red_curve, true);
                            break;
                        }

                        // if SHIFT key held switch to HANDLE tool
                        if ((event.modifiers & GDK_SHIFT_MASK) && is_bezier) {
                            state = PenTool::HANDLE;
                            red_curve.reset();
                            this->red_bpath->set_bpath(&red_curve, true);
                            break;
                        }

                        if (!anchor && !prev) { // Snap node only if not hitting anchor

                            if (hid_handles) {
                                cl1->set_visible(true);
                                fh_anchor->ctrl->set_visible(true);
                                hid_handles = false;
                            }
                            _endpointSnap(p, event.modifiers);
                            _setSubsequentPoint(p, true, event.modifiers);
                        } else if (green_anchor && green_anchor->active && green_curve && !green_curve->is_unset()) {
                            // The green anchor is the end point, use the initial point explicitly.
                            _setSubsequentPoint(green_curve->first_path()->initialPoint(), false, event.modifiers);
                        } else if (anchor) {
                            _setSubsequentPoint(anchor->dp, false, event.modifiers);
                        } else {
                            // if hovering over previous node, delete its front handles and the red curve
                            this->red_curve.reset();
                            this->red_bpath->set_bpath(&red_curve, true);
                            if (cl1->is_visible()) {
                                cl1->set_visible(false);
                                fh_anchor->ctrl->set_visible(false);
                                hid_handles = true;
                            }
                        }

                        if (anchor && !anchor_statusbar) {
                            Glib::ustring message =
                                (!is_spiro && !is_bspline) ? "" : "<b>Shift</b> + <b>Click</b> make a cusp node";
                            message_context->setF(
                                Inkscape::NORMAL_MESSAGE,
                                _("<b>Click</b> or <b>click and drag</b> to close and finish the path. %s"),
                                message.c_str());
                            anchor_statusbar = true;
                        } else if (!anchor && anchor_statusbar) {
                            message_context->clear();
                            anchor_statusbar = false;
                        }

                        if (prev && !prev_anchor_statusbar) {
                            Glib::ustring message = (!is_spiro && !is_bspline) ? "delete front handle of the previous"
                                                                               : "make last node a cusp";
                            message_context->setF(Inkscape::NORMAL_MESSAGE,
                                                  _("<b>Click</b> or <b>click and drag</b> to %s node."),
                                                  message.c_str());
                            prev_anchor_statusbar = true;
                        } else if (!prev && prev_anchor_statusbar) {
                            message_context->clear();
                            prev_anchor_statusbar = false;
                        }

                        ret = true;
                    } else {
                        if (anchor && !anchor_statusbar) {
                            Glib::ustring message =
                                (!is_spiro && !is_bspline) ? "" : "<b>Shift</b> + <b>Click</b> make a cusp node";
                            message_context->setF(
                                Inkscape::NORMAL_MESSAGE,
                                _("<b>Click</b> or <b>click and drag</b> to continue the path from this point. %s"),
                                message.c_str());
                            anchor_statusbar = true;
                        } else if (!anchor && anchor_statusbar) {
                            message_context->clear();
                            anchor_statusbar = false;
                        }

                        if (!sp_event_context_knot_mouseover()) {
                            SnapManager &m = _desktop->getNamedView()->snap_manager;
                            m.setup(_desktop);
                            m.preSnap(Inkscape::SnapCandidatePoint(p, Inkscape::SNAPSOURCE_NODE_HANDLE));
                            m.unSetup();
                        }
                    }
                    break;
                case PenTool::CONTROL:
                case PenTool::CLOSE:
                    // Placing controls is last operation in CLOSE state

                    // snap the handle
                    _endpointSnapHandle(p, event.modifiers);

                    if (!is_polylines_only) {
                        _setCtrl(p, event.modifiers);
                    } else {
                        _setCtrl(p_array[1], event.modifiers);
                    }

                    gobble_motion_events(GDK_BUTTON1_MASK);
                    ret = true;
                    break;
                case PenTool::BREAK:
                    break;
                case PenTool::NODE: {
                    // if we release ALT while dragging node, continue to drag
                    if (!(event.modifiers & GDK_ALT_MASK)) {
                        state = PenTool::POINT;

                        // make all anchors inactive
                        for (auto &_anchor : anchors) {
                            if (_anchor->active) {
                                _anchor->ctrl->set_normal();
                                _anchor->ctrl->set_size(Inkscape::HandleSize::NORMAL);
                                _anchor->active = false;

                                // There could be only one active anchor
                                break;
                            }
                        }

                        // Reset statusbar
                        if (node_mode_statusbar) {
                            node_mode_statusbar = false;
                            message_context->clear();
                        }

                        break;
                    }

                    // Setting the statusbar
                    if (!node_mode_statusbar) {
                        node_mode_statusbar = true;
                        message_context->set(Inkscape::NORMAL_MESSAGE,
                                             _("<b>Click</b> or <b>Click and drag</b> any node to move it."));
                    }

                    if (node_index == NONE_SELECTED) {
                        auto canvas_shape = make_canvasitem<Inkscape::CanvasItemBpath>(
                            _desktop->getCanvasSketch(), copy_pathvector_optional(green_curve), true);

                        for (auto &_anchor : anchors) {
                            if (_anchor->anchorTest(event_w, true)) {
                                // Highlight the node we hover over
                                break;
                            }
                        }
                    } else {
                        // User has clicked on a node
                        _moveNode(p);
                    }

                    break;
                }
                case PenTool::HANDLE: {
                    // if we release SHIFT while dragging handle, continue to drag
                    if (!(event.modifiers & GDK_SHIFT_MASK)) {
                        state = PenTool::POINT;
                        selected_anchor = nullptr;

                        if (drag_handle_statusbar) {
                            drag_handle_statusbar = false;
                            message_context->clear();
                        }

                        break;
                    }

                    // setting the statusbar
                    if (!drag_handle_statusbar) {
                        drag_handle_statusbar = true;
                        message_context->set(
                            Inkscape::NORMAL_MESSAGE,
                            _("<b>Click</b> or <b>Click and drag</b> any handle of last node to move it."));
                    }

                    if (!drag_handle)
                        selected_anchor = bh_anchor->anchorTest(event_w, true);
                    if (!selected_anchor)
                        selected_anchor = fh_anchor->anchorTest(event_w, true);

                    if (selected_anchor && drag_handle) {
                        _moveHandle(p);
                    }

                    break;
                }

                case PenTool::STOP:
                    // Don't break; fall through to default to do preSnapping
                default:
                    if (!sp_event_context_knot_mouseover()) {
                        SnapManager &m = _desktop->getNamedView()->snap_manager;
                        m.setup(_desktop);
                        m.preSnap(Inkscape::SnapCandidatePoint(p, Inkscape::SNAPSOURCE_NODE_HANDLE));
                        m.unSetup();
                    }
                    break;
            }
            break;
        default:
            break;
    }
    // calls the function "bspline_spiro_motion" when the mouse starts or stops moving
    if (is_bspline) {
        _bsplineSpiroMotion(event.modifiers);
    } else {
        if (Geom::LInfty(event_w - pen_drag_origin_w) > (tolerance / 2)) {
            _bsplineSpiroMotion(event.modifiers);
            pen_drag_origin_w = event_w;
        }
    }

    return ret;
}

/**
 * Handle mouse button release event.
 */
bool PenTool::_handleButtonRelease(ButtonReleaseEvent const &event)
{
    if (events_disabled) {
        // skip event processing if events are disabled
        return false;
    }

    bool ret = false;

    if (event.button == 1) {
        Geom::Point const event_w(event.pos);

        // Find desktop coordinates
        Geom::Point p = _desktop->w2d(event_w);

        // Test whether we hit any anchor.

        SPDrawAnchor *anchor = spdc_test_inside(this, event_w);
        // if we try to create a node in the same place as another node, we skip
        if ((!anchor || anchor == sa) && (is_spiro || is_bspline) && npoints > 0 && p_array[0] == p_array[3]) {
            return true;
        }

        SPDrawAnchor *prev = nullptr;
        if (!anchors.empty() && !is_polylines_only && !is_polylines_paraxial) {
            prev = anchors.back()->anchorTest(event_w, true);
        }

        switch (mode) {
            case PenTool::MODE_CLICK:
                switch (state) {
                    case PenTool::POINT:
                        ea = anchor;
                        if (anchor) {
                            p = anchor->dp;
                        }
                        state = PenTool::CONTROL;
                        break;
                    case PenTool::CONTROL:
                        // End current segment
                        _endpointSnap(p, event.modifiers);
                        _finishSegment(p, event.modifiers);
                        state = PenTool::POINT;
                        break;
                    case PenTool::CLOSE:
                        // End current segment
                        if (!anchor) { // Snap node only if not hitting anchor
                            _endpointSnap(p, event.modifiers);
                        }
                        _finishSegment(p, event.modifiers);
                        // hide the guide of the penultimate node when closing the curve
                        if (is_spiro) {
                            ctrl[FRONT_HANDLE]->set_visible(false);
                        }
                        _finish(true);
                        state = PenTool::POINT;
                        break;
                    case PenTool::BREAK:
                    case PenTool::NODE:
                    case PenTool::HANDLE:
                        break;
                    case PenTool::STOP:
                        // This is allowed, if we just canceled curve
                        state = PenTool::POINT;
                        break;
                    default:
                        break;
                }
                break;
            case PenTool::MODE_DRAG:
                switch (state) {
                    case PenTool::POINT:
                    case PenTool::CONTROL:
                        _endpointSnap(p, event.modifiers);
                        _finishSegment(p, event.modifiers);
                        break;
                    case PenTool::CLOSE:
                        _endpointSnap(p, event.modifiers);
                        _finishSegment(p, event.modifiers);
                        // hide the penultimate node guide when closing the curve
                        if (is_spiro) {
                            ctrl[FRONT_HANDLE]->set_visible(false);
                        }
                        if (green_closed) {
                            // finishing at the start anchor, close curve
                            _finish(true);
                        } else {
                            // finishing at some other anchor, finish curve but not close
                            _finish(false);
                        }
                        break;

                    case PenTool::BREAK:
                        // we clicked on previous node, make it a line
                        _lastpointToLine();

                        // hide front handles
                        cl1->set_visible(false);
                        ctrl[FRONT_HANDLE]->set_visible(false);
                        hid_handles = false;

                        state = PenTool::POINT;
                        break;
                    case PenTool::NODE:
                        node_index = NONE_SELECTED;
                        break;
                    case PenTool::HANDLE:
                        drag_handle = false;
                        selected_anchor = nullptr;
                    case PenTool::STOP:
                        // This is allowed, if we just cancelled curve
                        break;
                    default:
                        break;
                }
                state = PenTool::POINT;
                break;
            default:
                break;
        }

        ungrabCanvasEvents();

        ret = true;

        green_closed = false;
    }

    // TODO: can we be sure that the path was created correctly?
    // TODO: should we offer an option to collect the clicks in a list?
    if (expecting_clicks_for_LPE == 0 && hasWaitingLPE()) {
        setPolylineMode();

        Inkscape::Selection *selection = _desktop->getSelection();

        if (waiting_LPE) {
            // we have an already created LPE waiting for a path
            waiting_LPE->acceptParamPath(cast<SPPath>(selection->singleItem()));
            selection->add(waiting_item);
            waiting_LPE = nullptr;
        } else {
            // the case that we need to create a new LPE and apply it to the just-drawn path is
            // handled in spdc_check_for_and_apply_waiting_LPE() in draw-context.cpp
        }
    }

    return ret;
}

void PenTool::_redrawAll(bool const draw_red)
{
    // green
    if (!green_bpaths.empty()) {
        // remove old piecewise green canvasitems
        green_bpaths.clear();

        // one canvas bpath for all of green_curve
        auto canvas_shape =
            new Inkscape::CanvasItemBpath(_desktop->getCanvasSketch(), copy_pathvector_optional(green_curve), true);
        canvas_shape->set_stroke(green_color);
        canvas_shape->set_fill(0x0, SP_WIND_RULE_NONZERO);
        green_bpaths.emplace_back(canvas_shape);
    }
    if (green_anchor) {
        green_anchor->ctrl->set_position(green_anchor->dp);
    }

    if (draw_red) {
        red_curve.reset();
        red_curve.moveto(p_array[0]);
        red_curve.curveto(p_array[1], p_array[2], p_array[3]);
        red_bpath->set_bpath(&red_curve, true);
    }

    for (auto &c : ctrl) {
        c->set_visible(false);
    }
    // handles
    // hide the handlers in bspline and spiro modes
    if (p_array[0] != p_array[1] && !is_spiro && !is_bspline) {
        ctrl[FRONT_HANDLE]->set_position(p_array[1]);
        ctrl[FRONT_HANDLE]->set_visible(true);
        cl1->set_coords(p_array[0], p_array[1]);
        cl1->set_visible(true);
    } else {
        cl1->set_visible(false);
    }

    Geom::Curve const *last_seg = green_curve->last_segment();
    if (last_seg) {
        Geom::CubicBezier const *cubic = dynamic_cast<Geom::CubicBezier const *>(last_seg);
        // hide the handlers in bspline and spiro modes
        if (cubic && (*cubic)[2] != p_array[0] && !is_spiro && !is_bspline) {
            Geom::Point p2 = (*cubic)[2];
            ctrl[BACK_HANDLE]->set_position(p2);
            ctrl[BACK_HANDLE]->set_visible(true);
            cl0->set_coords(p2, p_array[0]);
            cl0->set_visible(true);
        } else {
            cl0->set_visible(false);
        }
    }

    // simply redraw the spiro. because its a redrawing, we don't call the global function,
    // but we call the redrawing at the ending.
    _bsplineSpiroBuild();
}

void PenTool::_lastpointMove(gdouble x, gdouble y)
{
    if (npoints != 5)
        return;

    y *= -_desktop->yaxisdir();
    auto delta = Geom::Point(x, y);

    auto prefs = Inkscape::Preferences::get();
    bool const rotated = prefs->getBool("/options/moverotated/value", true);
    if (rotated) {
        delta *= _desktop->current_rotation().inverse();
    }

    // green
    if (!green_curve->is_unset()) {
        green_curve->last_point_additive_move(delta);
    } else {
        // start anchor too
        if (green_anchor) {
            green_anchor->dp += delta;
        }
    }

    // red
    p_array[0] += delta;
    p_array[1] += delta;

    if (!anchors.empty()) {
        anchors.back()->dp = p_array[0];
        anchors.back()->ctrl->set_position(p_array[0]);
    }
    _redrawAll(true);
}

void PenTool::_lastpointMoveScreen(gdouble x, gdouble y)
{
    this->_lastpointMove(x / _desktop->current_zoom(), y / _desktop->current_zoom());
}

void PenTool::_lastpointToCurve()
{
    // avoid that if the "red_curve" contains only two points ( rect ), it doesn't stop here.
    if (this->npoints != 5 && !this->is_spiro && !this->is_bspline)
        return;

    p_array[1] = this->red_curve.last_segment()->initialPoint() +
                 (1. / 3.) * (*this->red_curve.last_point() - this->red_curve.last_segment()->initialPoint());
    // modificate the last segment of the green curve so it creates the type of node we need
    if (this->is_spiro || this->is_bspline) {
        if (!this->green_curve->is_unset()) {
            Geom::Point A(0, 0);
            Geom::Point B(0, 0);
            Geom::Point C(0, 0);
            Geom::Point D(0, 0);
            Geom::CubicBezier const *cubic = dynamic_cast<Geom::CubicBezier const *>(this->green_curve->last_segment());
            // We obtain the last segment 4 points in the previous curve
            if (cubic) {
                A = (*cubic)[0];
                B = (*cubic)[1];
                if (this->is_spiro) {
                    C = p_array[0] + (p_array[0] - p_array[1]);
                } else {
                    C = *this->green_curve->last_point() +
                        (1. / 3.) *
                            (this->green_curve->last_segment()->initialPoint() - *this->green_curve->last_point());
                }
                D = (*cubic)[3];
            } else {
                A = this->green_curve->last_segment()->initialPoint();
                B = this->green_curve->last_segment()->initialPoint();
                if (this->is_spiro) {
                    C = p_array[0] + (p_array[0] - p_array[1]);
                } else {
                    C = *this->green_curve->last_point() +
                        (1. / 3.) *
                            (this->green_curve->last_segment()->initialPoint() - *this->green_curve->last_point());
                }
                D = *this->green_curve->last_point();
            }
            auto previous = std::make_shared<SPCurve>();
            previous->moveto(A);
            previous->curveto(B, C, D);
            if (green_curve->get_segment_count() == 1) {
                green_curve = std::move(previous);
            } else {
                // we eliminate the last segment
                green_curve->backspace();
                // and we add it again with the recreation
                green_curve->append_continuous(*previous);
            }
        }
        // if the last node is an union with another curve
        if (this->green_curve->is_unset() && this->sa && !this->sa->curve->is_unset()) {
            this->_bsplineSpiroStartAnchor(false);
        }
    }

    this->_redrawAll(true);
}

void PenTool::_lastpointToLine()
{
    // avoid that if the "red_curve" contains only two points ( rect) it doesn't stop here.
    if (this->npoints != 5 && !this->is_bspline)
        return;

    // modify the last segment of the green curve so the type of node we want is created.
    if (this->is_spiro || this->is_bspline) {
        if (!this->green_curve->is_unset()) {
            Geom::Point A(0, 0);
            Geom::Point B(0, 0);
            Geom::Point C(0, 0);
            Geom::Point D(0, 0);
            auto previous = std::make_shared<SPCurve>();
            if (auto const cubic = dynamic_cast<Geom::CubicBezier const *>(green_curve->last_segment())) {
                A = green_curve->last_segment()->initialPoint();
                B = (*cubic)[1];
                C = *green_curve->last_point();
                D = C;
            } else {
                // We obtain the last segment 4 points in the previous curve
                A = green_curve->last_segment()->initialPoint();
                B = A;
                C = *green_curve->last_point();
                D = C;
            }
            previous->moveto(A);
            previous->curveto(B, C, D);
            if (green_curve->get_segment_count() == 1) {
                green_curve = std::move(previous);
            } else {
                // we eliminate the last segment
                green_curve->backspace();
                // and we add it again with the recreation
                green_curve->append_continuous(*previous);
            }
        }
        // if the last node is an union with another curve
        if (green_curve->is_unset() && sa && !sa->curve->is_unset()) {
            _bsplineSpiroStartAnchor(true);
        }
    }

    p_array[1] = p_array[0];
    // since we have a straight line now we need to change npoints
    npoints = 2;
    this->_redrawAll(true);
}

void PenTool::_moveHandle(Geom::Point const p)
{
    if (selected_anchor == fh_anchor.get()) {
        p_array[1] = p;
        fh_anchor->dp = p;
        fh_anchor->ctrl->set_position(p);
        cl1->set_coords(p_array[0], p_array[1]);
    }

    if (selected_anchor == bh_anchor.get()) {
        if (auto const last_seg = green_curve->last_segment()) {
            if (auto const cubic = dynamic_cast<Geom::CubicBezier const *>(last_seg)) {
                SPCurve lsegment;
                lsegment.moveto((*cubic)[0]);
                lsegment.curveto((*cubic)[1], p_array[0] + (p - (*cubic)[3]), p_array[0]);
                green_curve->backspace();
                green_curve->append_continuous(std::move(lsegment));
            }
        }

        bh_anchor->dp = p;
        bh_anchor->ctrl->set_position(p);
        cl0->set_coords(p_array[0], p);

        green_bpaths.clear();

        // one canvas bpath for all of green_curve
        auto canvas_shape =
            new Inkscape::CanvasItemBpath(_desktop->getCanvasSketch(), copy_pathvector_optional(green_curve), true);
        canvas_shape->set_stroke(green_color);
        canvas_shape->set_fill(0x0, SP_WIND_RULE_NONZERO);
        green_bpaths.emplace_back(canvas_shape);
    }

    return;
}

void PenTool::_moveNode(Geom::Point const p)
{
    if (node_index == NONE_SELECTED) {
        // This function should be called unless dragging a node
        return;
    }

    bool _after_exists = true;

    if (node_index == (anchors.size() - 1))
        _after_exists = false;

    Geom::Point delta((p - anchors[node_index]->dp).x(), (p - anchors[node_index]->dp).y());

    auto prefs = Inkscape::Preferences::get();
    bool const rotated = prefs->getBool("/options/moverotated/value", true);
    if (rotated) {
        delta *= Geom::Rotate(-_desktop->current_rotation().angle());
    }

    // Move green curve
    if (!green_curve->is_unset()) {
        green_curve->nth_point_additive_move(delta, node_index);
    } else {
        g_warning(" Green curve is unset ");
    }

    if (!_after_exists) {
        // Reset the anchors if last point on curve
        p_array[0] += delta;
        p_array[1] += delta;
    }

    _redrawAll(false);

    ctrl[FRONT_HANDLE]->set_visible(false);
    ctrl[BACK_HANDLE]->set_visible(false);

    if (!_after_exists) {
        fh_anchor->dp = ctrl[FRONT_HANDLE]->get_position();
        fh_anchor->ctrl->set_position(fh_anchor->dp);
        bh_anchor->dp = ctrl[BACK_HANDLE]->get_position();
        bh_anchor->ctrl->set_position(bh_anchor->dp);
    }

    // move the anchor
    anchors[node_index]->dp = p;
    anchors[node_index]->ctrl->set_position(p);

    return;
}

bool PenTool::_handleKeyPress(KeyPressEvent const &event)
{
    bool ret = false;
    auto prefs = Preferences::get();
    double const nudge = prefs->getDoubleLimited("/options/nudgedistance/value", 2, 0, 1000, "px"); // in px

    // Check for undo/redo.
    if (npoints > 0 && _acc_undo.isTriggeredBy(event)) {
        return _undoLastPoint(true);
    } else if (_acc_redo.isTriggeredBy(event)) {
        return _redoLastPoint();
    }
    if (_acc_to_line.isTriggeredBy(event)) {
        this->_lastpointToLine();
        ret = true;
    } else if (_acc_to_curve.isTriggeredBy(event)) {
        this->_lastpointToCurve();
        ret = true;
    }
    if (_acc_to_guides.isTriggeredBy(event)) {
        _desktop->getSelection()->toGuides();
        ret = true;
    }

    switch (get_latin_keyval(event)) {
        case GDK_KEY_Left: // move last point left
        case GDK_KEY_KP_Left:
            if (!mod_ctrl(event)) {   // not ctrl
                if (mod_alt(event)) { // alt
                    if (mod_shift(event)) {
                        this->_lastpointMoveScreen(-10, 0); // shift
                    } else {
                        this->_lastpointMoveScreen(-1, 0); // no shift
                    }
                } else { // no alt
                    if (mod_shift(event)) {
                        this->_lastpointMove(-10 * nudge, 0); // shift
                    } else {
                        this->_lastpointMove(-nudge, 0); // no shift
                    }
                }
                ret = true;
            }
            break;
        case GDK_KEY_Up: // move last point up
        case GDK_KEY_KP_Up:
            if (!mod_ctrl(event)) {   // not ctrl
                if (mod_alt(event)) { // alt
                    if (mod_shift(event)) {
                        this->_lastpointMoveScreen(0, 10); // shift
                    } else {
                        this->_lastpointMoveScreen(0, 1); // no shift
                    }
                } else { // no alt
                    if (mod_shift(event)) {
                        this->_lastpointMove(0, 10 * nudge); // shift
                    } else {
                        this->_lastpointMove(0, nudge); // no shift
                    }
                }
                ret = true;
            }
            break;
        case GDK_KEY_Right: // move last point right
        case GDK_KEY_KP_Right:
            if (!mod_ctrl(event)) {   // not ctrl
                if (mod_alt(event)) { // alt
                    if (mod_shift(event)) {
                        this->_lastpointMoveScreen(10, 0); // shift
                    } else {
                        this->_lastpointMoveScreen(1, 0); // no shift
                    }
                } else { // no alt
                    if (mod_shift(event)) {
                        this->_lastpointMove(10 * nudge, 0); // shift
                    } else {
                        this->_lastpointMove(nudge, 0); // no shift
                    }
                }
                ret = true;
            }
            break;
        case GDK_KEY_Down: // move last point down
        case GDK_KEY_KP_Down:
            if (!mod_ctrl(event)) {   // not ctrl
                if (mod_alt(event)) { // alt
                    if (mod_shift(event)) {
                        this->_lastpointMoveScreen(0, -10); // shift
                    } else {
                        this->_lastpointMoveScreen(0, -1); // no shift
                    }
                } else { // no alt
                    if (mod_shift(event)) {
                        this->_lastpointMove(0, -10 * nudge); // shift
                    } else {
                        this->_lastpointMove(0, -nudge); // no shift
                    }
                }
                ret = true;
            }
            break;

            /*TODO: this is not yet enabled?? looks like some traces of the Geometry tool
                    case GDK_KEY_P:
                    case GDK_KEY_p:
                        if (MOD__SHIFT_ONLY(event)) {
                            sp_pen_context_wait_for_LPE_mouse_clicks(pc, Inkscape::LivePathEffect::PARALLEL, 2);
                            ret = true;
                        }
                        break;

                    case GDK_KEY_C:
                    case GDK_KEY_c:
                        if (MOD__SHIFT_ONLY(event)) {
                            sp_pen_context_wait_for_LPE_mouse_clicks(pc, Inkscape::LivePathEffect::CIRCLE_3PTS, 3);
                            ret = true;
                        }
                        break;

                    case GDK_KEY_B:
                    case GDK_KEY_b:
                        if (MOD__SHIFT_ONLY(event)) {
                            sp_pen_context_wait_for_LPE_mouse_clicks(pc, Inkscape::LivePathEffect::PERP_BISECTOR, 2);
                            ret = true;
                        }
                        break;

                    case GDK_KEY_A:
                    case GDK_KEY_a:
                        if (MOD__SHIFT_ONLY(event)) {
                            sp_pen_context_wait_for_LPE_mouse_clicks(pc, Inkscape::LivePathEffect::ANGLE_BISECTOR, 3);
                            ret = true;
                        }
                        break;
            */

        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter:
            if (this->npoints != 0) {
                this->ea = nullptr; // unset end anchor if set (otherwise crashes)
                if (mod_shift_only(event)) {
                    // All this is needed to stop the last control
                    // point dispeating and stop making an n-1 shape.
                    Geom::Point const p(0, 0);
                    if (this->red_curve.is_unset()) {
                        this->red_curve.moveto(p);
                    }
                    this->_finishSegment(p, 0);
                    this->_finish(true);
                } else {
                    this->_finish(false);
                }
                ret = true;
            }
            break;
        case GDK_KEY_Escape:
            if (this->npoints != 0) {
                // if drawing, cancel, otherwise pass it up for deselecting
                this->_cancel();
                ret = true;
            }
            break;
        case GDK_KEY_BackSpace:
        case GDK_KEY_Delete:
        case GDK_KEY_KP_Delete:
            ret = _undoLastPoint();
            break;
        default:
            break;
    }
    return ret;
}

void PenTool::_resetColors()
{
    // Red
    this->red_curve.reset();
    this->red_bpath->set_bpath(nullptr);

    // Blue
    blue_curve.reset();
    blue_bpath->set_bpath(nullptr);

    // Green
    this->green_bpaths.clear();
    this->green_curve->reset();
    this->green_anchor.reset();

    this->sa = nullptr;
    this->ea = nullptr;

    if (this->sa_overwrited) {
        this->sa_overwrited->reset();
    }

    this->npoints = 0;
    this->red_curve_is_valid = false;
}

void PenTool::_setInitialPoint(Geom::Point const p)
{
    g_assert(this->npoints == 0);

    p_array[0] = p;
    p_array[1] = p;
    this->npoints = 2;
    this->red_bpath->set_bpath(nullptr);
}

/**
 * Show the status message for the current line/curve segment.
 * This type of message always shows angle/distance as the last
 * two parameters ("angle %3.2f&#176;, distance %s").
 */
void PenTool::_setAngleDistanceStatusMessage(Geom::Point const p, int pc_point_to_compare, gchar const *message)
{
    g_assert((pc_point_to_compare == 0) || (pc_point_to_compare == 3)); // exclude control handles
    g_assert(message != nullptr);

    Geom::Point rel = p - p_array[pc_point_to_compare];
    Inkscape::Util::Quantity q = Inkscape::Util::Quantity(Geom::L2(rel), "px");
    Glib::ustring dist = q.string(_desktop->getNamedView()->display_units);
    double angle = atan2(rel[Geom::Y], rel[Geom::X]) * 180 / M_PI;
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    if (prefs->getBool("/options/compassangledisplay/value", false) != 0) {
        angle = 90 - angle;

        if (_desktop->is_yaxisdown()) {
            angle = 180 - angle;
        }

        if (angle < 0) {
            angle += 360;
        }
    }

    this->message_context->setF(Inkscape::IMMEDIATE_MESSAGE, message, angle, dist.c_str());
}

// this function changes the colors red, green and blue making them transparent or not, depending on if spiro is being
// used.
void PenTool::_bsplineSpiroColor()
{
    static Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    auto highlight = currentLayer()->highlight_color();
    auto other = prefs->getColor("/tools/nodes/highlight_color", "#ff0000ff");
    if (this->is_spiro) {
        this->red_color = 0xff000000;
        this->green_color = 0x00ff0000;
    } else if (this->is_bspline) {
        highlight_color = highlight.toRGBA();
        if (other == highlight) {
            this->green_color = 0xff00007f;
            this->red_color = 0xff00007f;
        } else {
            this->green_color = this->highlight_color;
            this->red_color = this->highlight_color;
        }
    } else {
        highlight_color = highlight.toRGBA();
        this->red_color = 0xff00007f;
        if (other == highlight) {
            this->green_color = 0x00ff007f;
        } else {
            this->green_color = this->highlight_color;
        }
        blue_bpath->set_visible(false);
    }

    // We erase all the "green_bpaths" to recreate them after with the colour
    // transparency recently modified
    if (!this->green_bpaths.empty()) {
        // remove old piecewise green canvasitems
        this->green_bpaths.clear();

        // one canvas bpath for all of green_curve
        auto canvas_shape =
            new Inkscape::CanvasItemBpath(_desktop->getCanvasSketch(), copy_pathvector_optional(green_curve), true);
        canvas_shape->set_stroke(green_color);
        canvas_shape->set_fill(0x0, SP_WIND_RULE_NONZERO);
        green_bpaths.emplace_back(canvas_shape);
    }

    this->red_bpath->set_stroke(red_color);
}

void PenTool::_bsplineSpiro(bool shift)
{
    if (!this->is_spiro && !this->is_bspline) {
        return;
    }

    shift ? this->_bsplineSpiroOff() : this->_bsplineSpiroOn();
    this->_bsplineSpiroBuild();
}

void PenTool::_bsplineSpiroOn()
{
    if (!this->red_curve.is_unset()) {
        this->npoints = 5;
        p_array[0] = *this->red_curve.first_point();
        p_array[3] = this->red_curve.first_segment()->finalPoint();
        p_array[2] = p_array[3] + (1. / 3) * (p_array[0] - p_array[3]);
        _bsplineSpiroMotion(GDK_ALT_MASK);
    }
}

void PenTool::_bsplineSpiroOff()
{
    if (!this->red_curve.is_unset()) {
        this->npoints = 5;
        p_array[0] = *this->red_curve.first_point();
        p_array[3] = this->red_curve.first_segment()->finalPoint();
        p_array[2] = p_array[3];
    }
}

void PenTool::_bsplineSpiroStartAnchor(bool shift)
{
    if (this->sa->curve->is_unset()) {
        return;
    }

    LivePathEffect::LPEBSpline *lpe_bsp = nullptr;

    if (is<SPLPEItem>(this->white_item) && cast<SPLPEItem>(this->white_item)->hasPathEffect()) {
        Inkscape::LivePathEffect::Effect *thisEffect =
            cast<SPLPEItem>(this->white_item)->getFirstPathEffectOfType(Inkscape::LivePathEffect::BSPLINE);
        if (thisEffect) {
            lpe_bsp = dynamic_cast<LivePathEffect::LPEBSpline *>(thisEffect->getLPEObj()->get_lpe());
        }
    }
    if (lpe_bsp) {
        this->is_bspline = true;
    } else {
        this->is_bspline = false;
    }
    LivePathEffect::LPESpiro *lpe_spi = nullptr;

    if (is<SPLPEItem>(this->white_item) && cast<SPLPEItem>(this->white_item)->hasPathEffect()) {
        Inkscape::LivePathEffect::Effect *thisEffect =
            cast<SPLPEItem>(this->white_item)->getFirstPathEffectOfType(Inkscape::LivePathEffect::SPIRO);
        if (thisEffect) {
            lpe_spi = dynamic_cast<LivePathEffect::LPESpiro *>(thisEffect->getLPEObj()->get_lpe());
        }
    }
    if (lpe_spi) {
        this->is_spiro = true;
    } else {
        this->is_spiro = false;
    }
    if (!this->is_spiro && !this->is_bspline) {
        _bsplineSpiroColor();
        return;
    }
    if (shift) {
        this->_bsplineSpiroStartAnchorOff();
    } else {
        this->_bsplineSpiroStartAnchorOn();
    }
}

void PenTool::_bsplineSpiroStartAnchorOn()
{
    using Geom::X;
    using Geom::Y;
    Geom::CubicBezier const *cubic = dynamic_cast<Geom::CubicBezier const *>(&*this->sa_overwrited->last_segment());
    auto last_segment = std::make_shared<SPCurve>();
    Geom::Point point_a = this->sa_overwrited->last_segment()->initialPoint();
    Geom::Point point_d = *this->sa_overwrited->last_point();
    Geom::Point point_c = point_d + (1. / 3) * (point_a - point_d);
    if (cubic) {
        last_segment->moveto(point_a);
        last_segment->curveto((*cubic)[1], point_c, point_d);
    } else {
        last_segment->moveto(point_a);
        last_segment->curveto(point_a, point_c, point_d);
    }
    if (this->sa_overwrited->get_segment_count() == 1) {
        this->sa_overwrited = std::move(last_segment);
    } else {
        // we eliminate the last segment
        this->sa_overwrited->backspace();
        // and we add it again with the recreation
        sa_overwrited->append_continuous(*last_segment);
    }
}

void PenTool::_bsplineSpiroStartAnchorOff()
{
    Geom::CubicBezier const *cubic = dynamic_cast<Geom::CubicBezier const *>(&*this->sa_overwrited->last_segment());
    if (cubic) {
        auto last_segment = std::make_shared<SPCurve>();
        last_segment->moveto((*cubic)[0]);
        last_segment->curveto((*cubic)[1], (*cubic)[3], (*cubic)[3]);
        if (this->sa_overwrited->get_segment_count() == 1) {
            this->sa_overwrited = std::move(last_segment);
        } else {
            // we eliminate the last segment
            this->sa_overwrited->backspace();
            // and we add it again with the recreation
            sa_overwrited->append_continuous(*last_segment);
        }
    }
}

void PenTool::_bsplineSpiroMotion(guint const state)
{
    bool shift = state & GDK_SHIFT_MASK;
    if (!this->is_spiro && !this->is_bspline) {
        return;
    }
    using Geom::X;
    using Geom::Y;
    if (this->red_curve.is_unset())
        return;
    this->npoints = 5;
    SPCurve tmp_curve;
    p_array[2] = p_array[3] + (1. / 3) * (p_array[0] - p_array[3]);
    if (this->green_curve->is_unset() && !this->sa) {
        p_array[1] = p_array[0] + (1. / 3) * (p_array[3] - p_array[0]);
        if (shift) {
            p_array[2] = p_array[3];
        }
    } else if (!this->green_curve->is_unset()) {
        tmp_curve = *green_curve;
    } else {
        tmp_curve = *sa_overwrited;
    }
    if ((state & GDK_ALT_MASK) && previous != Geom::Point(0, 0)) { // ALT drag
        p_array[0] = p_array[0] + (p_array[3] - previous);
    }
    if (!tmp_curve.is_unset()) {
        Geom::CubicBezier const *cubic = dynamic_cast<Geom::CubicBezier const *>(tmp_curve.last_segment());
        if ((state & GDK_ALT_MASK) && !Geom::are_near(*tmp_curve.last_point(), p_array[0], 0.1)) {
            SPCurve previous_weight_power;
            previous_weight_power.moveto(tmp_curve.last_segment()->initialPoint());
            previous_weight_power.lineto(p_array[0]);
            auto SBasisweight_power = previous_weight_power.first_segment()->toSBasis();
            if (tmp_curve.get_segment_count() == 1) {
                Geom::Point initial = tmp_curve.last_segment()->initialPoint();
                tmp_curve.reset();
                tmp_curve.moveto(initial);
            } else {
                tmp_curve.backspace();
            }
            if (this->is_bspline && cubic && !Geom::are_near((*cubic)[2], (*cubic)[3])) {
                tmp_curve.curveto(SBasisweight_power.valueAt(0.33334), SBasisweight_power.valueAt(0.66667), p_array[0]);
            } else if (this->is_bspline && cubic) {
                tmp_curve.curveto(SBasisweight_power.valueAt(0.33334), p_array[0], p_array[0]);
            } else if (cubic && !Geom::are_near((*cubic)[2], (*cubic)[3])) {
                tmp_curve.curveto((*cubic)[1], (*cubic)[2] + (p_array[3] - previous), p_array[0]);
            } else if (cubic) {
                tmp_curve.curveto((*cubic)[1], p_array[0], p_array[0]);
            } else {
                tmp_curve.lineto(p_array[0]);
            }
            cubic = dynamic_cast<Geom::CubicBezier const *>(tmp_curve.last_segment());
            if (sa && green_curve->is_unset()) {
                sa_overwrited = std::make_shared<SPCurve>(tmp_curve);
            }
            green_curve = std::make_shared<SPCurve>(std::move(tmp_curve));
        }
        if (cubic) {
            if (this->is_bspline) {
                SPCurve weight_power;
                weight_power.moveto(red_curve.last_segment()->initialPoint());
                weight_power.lineto(*red_curve.last_point());
                auto SBasisweight_power = weight_power.first_segment()->toSBasis();
                p_array[1] = SBasisweight_power.valueAt(0.33334);
                if (Geom::are_near(p_array[1], p_array[0])) {
                    p_array[1] = p_array[0];
                }
                if (shift) {
                    p_array[2] = p_array[3];
                }
                if (Geom::are_near((*cubic)[3], (*cubic)[2])) {
                    p_array[1] = p_array[0];
                }
            } else {
                p_array[1] = (*cubic)[3] + ((*cubic)[3] - (*cubic)[2]);
            }
        } else {
            p_array[1] = p_array[0];
            if (shift) {
                p_array[2] = p_array[3];
            }
        }
        previous = *red_curve.last_point();
        SPCurve red;
        red.moveto(p_array[0]);
        red.curveto(p_array[1], p_array[2], p_array[3]);
        red_bpath->set_bpath(&red, true);
    }

    if (this->anchor_statusbar && !this->red_curve.is_unset()) {
        if (shift) {
            this->_bsplineSpiroEndAnchorOff();
        } else {
            this->_bsplineSpiroEndAnchorOn();
        }
    }

    // update position of old spiro anchor
    if (!anchors.empty()) {
        anchors.back()->dp = p_array[0];
        anchors.back()->ctrl->set_position(p_array[0]);
    }

    // remove old piecewise green canvasitems
    green_bpaths.clear();

    // one canvas bpath for all of green_curve
    auto canvas_shape =
        new Inkscape::CanvasItemBpath(_desktop->getCanvasSketch(), copy_pathvector_optional(green_curve), true);
    canvas_shape->set_stroke(green_color);
    canvas_shape->set_fill(0x0, SP_WIND_RULE_NONZERO);
    green_bpaths.emplace_back(canvas_shape);

    this->_bsplineSpiroBuild();
}

void PenTool::_bsplineSpiroEndAnchorOn()
{
    using Geom::X;
    using Geom::Y;
    p_array[2] = p_array[3] + (1. / 3) * (p_array[0] - p_array[3]);
    SPCurve tmp_curve;
    SPCurve last_segment;
    Geom::Point point_c(0, 0);
    if (green_anchor && green_anchor->active) {
        tmp_curve = green_curve->reversed();
        if (green_curve->get_segment_count() == 0) {
            return;
        }
    } else if (this->sa) {
        tmp_curve = sa_overwrited->reversed();
    } else {
        return;
    }
    Geom::CubicBezier const *cubic = dynamic_cast<Geom::CubicBezier const *>(tmp_curve.last_segment());
    if (this->is_bspline) {
        point_c =
            *tmp_curve.last_point() + (1. / 3) * (tmp_curve.last_segment()->initialPoint() - *tmp_curve.last_point());
    } else {
        point_c = p_array[3] + p_array[3] - p_array[2];
    }
    if (cubic) {
        last_segment.moveto((*cubic)[0]);
        last_segment.curveto((*cubic)[1], point_c, (*cubic)[3]);
    } else {
        last_segment.moveto(tmp_curve.last_segment()->initialPoint());
        last_segment.lineto(*tmp_curve.last_point());
    }
    if (tmp_curve.get_segment_count() == 1) {
        tmp_curve = std::move(last_segment);
    } else {
        // we eliminate the last segment
        tmp_curve.backspace();
        // and we add it again with the recreation
        tmp_curve.append_continuous(std::move(last_segment));
    }
    tmp_curve.reverse();
    if (green_anchor && green_anchor->active) {
        green_curve->reset();
        green_curve = std::make_shared<SPCurve>(std::move(tmp_curve));
    } else {
        sa_overwrited->reset();
        sa_overwrited = std::make_shared<SPCurve>(std::move(tmp_curve));
    }
}

void PenTool::_bsplineSpiroEndAnchorOff()
{
    SPCurve tmp_curve;
    SPCurve last_segment;
    p_array[2] = p_array[3];
    if (green_anchor && green_anchor->active) {
        tmp_curve = green_curve->reversed();
        if (green_curve->get_segment_count() == 0) {
            return;
        }
    } else if (sa) {
        tmp_curve = sa_overwrited->reversed();
    } else {
        return;
    }
    Geom::CubicBezier const *cubic = dynamic_cast<Geom::CubicBezier const *>(tmp_curve.last_segment());
    if (cubic) {
        last_segment.moveto((*cubic)[0]);
        last_segment.curveto((*cubic)[1], (*cubic)[3], (*cubic)[3]);
    } else {
        last_segment.moveto(tmp_curve.last_segment()->initialPoint());
        last_segment.lineto(*tmp_curve.last_point());
    }
    if (tmp_curve.get_segment_count() == 1) {
        tmp_curve = std::move(last_segment);
    } else {
        // we eliminate the last segment
        tmp_curve.backspace();
        // and we add it again with the recreation
        tmp_curve.append_continuous(std::move(last_segment));
    }
    tmp_curve.reverse();

    if (green_anchor && green_anchor->active) {
        green_curve->reset();
        green_curve = std::make_shared<SPCurve>(std::move(tmp_curve));
    } else {
        sa_overwrited->reset();
        sa_overwrited = std::make_shared<SPCurve>(std::move(tmp_curve));
    }
}

// prepares the curves for its transformation into BSpline curve.
void PenTool::_bsplineSpiroBuild()
{
    if (!is_spiro && !is_bspline) {
        return;
    }

    // We create the base curve
    SPCurve curve;
    // If we continuate the existing curve we add it at the start
    if (sa && !sa->curve->is_unset()) {
        curve = *sa_overwrited;
    }

    if (!green_curve->is_unset()) {
        curve.append_continuous(*green_curve);
    }

    // and the red one
    if (!this->red_curve.is_unset()) {
        this->red_curve.reset();
        this->red_curve.moveto(p_array[0]);
        if (this->anchor_statusbar && !this->sa && !(this->green_anchor && this->green_anchor->active)) {
            this->red_curve.curveto(p_array[1], p_array[3], p_array[3]);
        } else {
            this->red_curve.curveto(p_array[1], p_array[2], p_array[3]);
        }
        red_bpath->set_bpath(&red_curve, true);
        curve.append_continuous(red_curve);
    }
    previous = *this->red_curve.last_point();
    if (!curve.is_unset()) {
        // close the curve if the final points of the curve are close enough
        if (Geom::are_near(curve.first_path()->initialPoint(), curve.last_path()->finalPoint())) {
            curve.closepath_current();
        }
        // TODO: CALL TO CLONED FUNCTION SPIRO::doEffect IN lpe-spiro.cpp
        // For example
        // using namespace Inkscape::LivePathEffect;
        // LivePathEffectObject *lpeobj = static_cast<LivePathEffectObject*> (curve);
        // Effect *spr = static_cast<Effect*> ( new LPEbspline(lpeobj) );
        // spr->doEffect(curve);
        if (is_bspline) {
            Inkscape::Preferences *prefs = Inkscape::Preferences::get();
            Geom::PathVector hp;
            bool uniform = false;
            Glib::ustring pref_path = "/live_effects/bspline/uniform";
            if (prefs->getEntry(pref_path).isValid()) {
                uniform = prefs->getString(pref_path) == "true";
            }
            LivePathEffect::sp_bspline_do_effect(curve, 0, hp, uniform);
        } else {
            LivePathEffect::sp_spiro_do_effect(curve);
        }

        blue_bpath->set_bpath(&curve, true);
        blue_bpath->set_stroke(blue_color);
        blue_bpath->set_visible(true);

        blue_curve.reset();
        // We hide the holders that doesn't contribute anything
        for (auto &c : ctrl) {
            c->set_visible(false);
        }
        if (is_spiro) {
            ctrl[FRONT_HANDLE]->set_position(p_array[0]);
            ctrl[FRONT_HANDLE]->set_visible(true);
        }
        cl0->set_visible(false);
        cl1->set_visible(false);
    } else {
        // if the curve is empty
        blue_bpath->set_visible(false);
    }
}

void PenTool::_setSubsequentPoint(Geom::Point const p, bool statusbar, guint status)
{
    g_assert(this->npoints != 0);

    // todo: Check callers to see whether 2 <= npoints is guaranteed.

    p_array[2] = p;
    p_array[3] = p;
    p_array[4] = p;
    this->npoints = 5;
    this->red_curve.reset();
    bool is_curve;
    this->red_curve.moveto(p_array[0]);
    if (this->is_polylines_paraxial && !statusbar) {
        // we are drawing horizontal/vertical lines and hit an anchor;
        Geom::Point const origin = p_array[0];
        // if the previous point and the anchor are not aligned either horizontally or vertically...
        if ((std::abs(p[Geom::X] - origin[Geom::X]) > 1e-9) && (std::abs(p[Geom::Y] - origin[Geom::Y]) > 1e-9)) {
            // ...then we should draw an L-shaped path, consisting of two paraxial segments
            Geom::Point intermed = p;
            this->_setToNearestHorizVert(intermed, status);
            this->red_curve.lineto(intermed);
        }
        this->red_curve.lineto(p);
        is_curve = false;
    } else {
        // one of the 'regular' modes
        if (p_array[1] != p_array[0] || this->is_spiro) {
            this->red_curve.curveto(p_array[1], p, p);
            is_curve = true;
        } else {
            this->red_curve.lineto(p);
            is_curve = false;
        }
    }

    red_bpath->set_bpath(&red_curve, true);

    if (statusbar) {
        gchar *message;
        if (this->is_spiro || this->is_bspline) {
            message = is_curve ? _("<b>Curve segment</b>: angle %3.2f&#176;; <b>Shift+Click</b> creates cusp node, "
                                   "<b>ALT</b> moves previous, <b>Enter</b> or <b>Shift+Enter</b> to finish")
                               : _("<b>Line segment</b>: angle %3.2f&#176;; <b>Shift+Click</b> creates cusp node, "
                                   "<b>ALT</b> moves previous, <b>Enter</b> or <b>Shift+Enter</b> to finish");
            this->_setAngleDistanceStatusMessage(p, 0, message);
        } else {
            message = is_curve ? _("<b>Curve segment</b>: angle %3.2f&#176;, distance %s; with <b>Ctrl</b> to snap "
                                   "angle, <b>Enter</b> or <b>Shift+Enter</b> to finish the path, <b>Shift</b> to "
                                   "change last handles, <b>Alt</b> to move previous nodes")
                               : _("<b>Line segment</b>: angle %3.2f&#176;, distance %s; with <b>Ctrl</b> to snap "
                                   "angle, <b>Enter</b> or <b>Shift+Enter</b> to finish the path");
            this->_setAngleDistanceStatusMessage(p, 0, message);
        }
    }
}

void PenTool::_setCtrl(Geom::Point const q, guint const state)
{
    // use 'q' as 'p' use to shadow member variable.
    for (auto &c : ctrl) {
        c->set_visible(false);
    }

    // hide previous handle anchors
    fh_anchor->ctrl->set_visible(false);
    bh_anchor->ctrl->set_visible(false);
    ctrl[FRONT_HANDLE]->set_visible(true);
    cl1->set_visible(true);

    if (this->npoints == 2) {
        p_array[1] = q;
        cl0->set_visible(false);
        ctrl[FRONT_HANDLE]->set_position(p_array[1]);
        ctrl[FRONT_HANDLE]->set_visible(true);
        cl1->set_coords(p_array[0], p_array[1]);
        this->_setAngleDistanceStatusMessage(
            q, 0, _("<b>Curve handle</b>: angle %3.2f&#176;, length %s; with <b>Ctrl</b> to snap angle"));
    } else if (this->npoints == 5) {
        p_array[4] = q;
        cl0->set_visible(true);
        bool is_symm = false;
        if (((this->mode == PenTool::MODE_CLICK) && (state & GDK_CONTROL_MASK)) ||
            ((this->mode == PenTool::MODE_DRAG) && !(state & GDK_SHIFT_MASK))) {
            Geom::Point delta = q - p_array[3];
            if (this->mode == PenTool::MODE_DRAG && (state & GDK_ALT_MASK)) {
                // with Alt, we unlink handle length keeping directions opposite to each other
                p_array[2] = p_array[3] - (p_array[3] - p_array[2]).length() * Geom::unit_vector(delta);
            } else {
                p_array[2] = p_array[3] - delta;
                is_symm = true;
            }
            this->red_curve.reset();
            this->red_curve.moveto(p_array[0]);
            this->red_curve.curveto(p_array[1], p_array[2], p_array[3]);
            red_bpath->set_bpath(&red_curve, true);
        }

        if (((this->mode == PenTool::MODE_DRAG) && (state & GDK_SHIFT_MASK) && (state & GDK_ALT_MASK))) {
            // Alt + Shift is held we need to move the path
            p_array[3] = q - front_handle;
            p_array[2] = p_array[3] + back_handle;

            // Changing the red curve to match
            this->red_curve.reset();
            this->red_curve.moveto(p_array[0]);
            this->red_curve.curveto(p_array[1], p_array[2], p_array[3]);
            red_bpath->set_bpath(&red_curve, true);
        } else {
            front_handle = p_array[4] - p_array[3];
            back_handle = p_array[2] - p_array[3];
        }

        // Avoid conflicting with initial point ctrl
        ctrl[TEMPORARY_ANCHOR]->set_position(p_array[3]);
        ctrl[TEMPORARY_ANCHOR]->set_visible(true);
        ctrl[BACK_HANDLE]->set_position(p_array[2]);
        ctrl[BACK_HANDLE]->set_visible(true);
        ctrl[FRONT_HANDLE]->set_position(p_array[4]);
        ctrl[FRONT_HANDLE]->set_visible(true);

        cl0->set_coords(p_array[3], p_array[2]);
        cl1->set_coords(p_array[3], p_array[4]);

        gchar *message = is_symm ? _("<b>Curve handle, symmetric</b>: angle %3.2f&#176;, length %s; with <b>Ctrl</b> "
                                     "to snap angle, with <b>Shift</b> to break this handle, with <b>Alt</b> to unlink "
                                     "handle, with <b>Alt + Shift</b> to move node")
                                 : _("<b>Curve handle</b>: angle %3.2f&#176;, length %s; with <b>Ctrl</b> to snap "
                                     "angle, with <b>Shift</b> to break this handle, with <b>Alt</b> to unlink handle, "
                                     "with <b>Alt + Shift</b> to move node");
        this->_setAngleDistanceStatusMessage(q, 3, message);
    } else {
        g_warning("Something bad happened - npoints is %d", this->npoints);
    }
}

void PenTool::_finishSegment(Geom::Point const q, guint const state)
{ // use 'q' as 'p' shadows member variable.
    if (this->is_polylines_paraxial) {
        this->nextParaxialDirection(q, p_array[0], state);
    }

    if (!this->red_curve.is_unset()) {
        this->_bsplineSpiro(state & GDK_SHIFT_MASK);
        if (!this->green_curve->is_unset() && !Geom::are_near(*this->green_curve->last_point(), p_array[0])) {
            SPCurve lsegment;
            Geom::CubicBezier const *cubic =
                dynamic_cast<Geom::CubicBezier const *>(&*this->green_curve->last_segment());
            if (cubic) {
                lsegment.moveto((*cubic)[0]);
                lsegment.curveto((*cubic)[1], p_array[0] - ((*cubic)[2] - (*cubic)[3]), *this->red_curve.first_point());
                green_curve->backspace();
                green_curve->append_continuous(std::move(lsegment));
            }
        }
        green_curve->append_continuous(red_curve);
        auto curve = red_curve;

        /// \todo fixme:
        auto canvas_shape = new Inkscape::CanvasItemBpath(_desktop->getCanvasSketch(), curve.get_pathvector(), true);
        canvas_shape->set_stroke(green_color);
        canvas_shape->set_fill(0x0, SP_WIND_RULE_NONZERO);
        green_bpaths.emplace_back(canvas_shape);

        // display the new point
        anchors.push_back(std::make_unique<SPDrawAnchor>(this, green_curve, true, p_array[3]));
        if (is_bspline || is_spiro)
            anchors.back()->ctrl->set_type(CANVAS_ITEM_CTRL_TYPE_ROTATE);
        ctrl[TEMPORARY_ANCHOR]->set_visible(false);

        // hide control handles
        ctrl[FRONT_HANDLE]->set_visible(false);
        ctrl[BACK_HANDLE]->set_visible(false);

        // show new anchors
        fh_anchor->ctrl->set_position(p_array[4]);
        fh_anchor->dp = p_array[4];
        fh_anchor->ctrl->set_visible(true);
        if (is_bezier) {
            bh_anchor->ctrl->set_position(p_array[2]);
            bh_anchor->dp = p_array[2];
            bh_anchor->ctrl->set_visible(true);
        }

        p_array[0] = p_array[3];
        p_array[1] = p_array[4];
        this->npoints = 2;

        red_curve.reset();
        _redo_stack.clear();
    }
}

bool PenTool::_undoLastPoint(bool user_undo)
{
    bool ret = false;

    // remove last point from anchors
    if (!anchors.empty()) {
        anchors.pop_back();
    }

    // hide the anchors
    fh_anchor->ctrl->set_visible(false);
    bh_anchor->ctrl->set_visible(false);

    if (this->green_curve->is_unset() || (this->green_curve->last_segment() == nullptr)) {
        if (red_curve.is_unset()) {
            return ret; // do nothing; this event should be handled upstream
        }
        _cancel();
        ret = true;
    } else {
        red_curve.reset();
        if (user_undo) {
            if (_did_redo) {
                _redo_stack.clear();
                _did_redo = false;
            }
            _redo_stack.push_back(green_curve->get_pathvector());
        }
        // The code below assumes that this->green_curve has only ONE path !
        Geom::Curve const *crv = this->green_curve->last_segment();
        p_array[0] = crv->initialPoint();
        if (Geom::CubicBezier const *cubic = dynamic_cast<Geom::CubicBezier const *>(crv)) {
            p_array[1] = (*cubic)[1];

        } else {
            p_array[1] = p_array[0];
        }

        // assign the value in a third of the distance of the last segment.
        if (this->is_bspline) {
            p_array[1] = p_array[0] + (1. / 3) * (p_array[3] - p_array[0]);
        }

        Geom::Point const pt((this->npoints < 4) ? crv->finalPoint() : p_array[3]);

        this->npoints = 2;
        // delete the last segment of the green curve and green bpath
        if (this->green_curve->get_segment_count() == 1) {
            this->npoints = 5;
            if (!this->green_bpaths.empty()) {
                this->green_bpaths.pop_back();
            }
            this->green_curve->reset();
        } else {
            this->green_curve->backspace();
            if (this->green_bpaths.size() > 1) {
                this->green_bpaths.pop_back();
            } else if (this->green_bpaths.size() == 1) {
                green_bpaths.back()->set_bpath(green_curve.get(), true);
            }
        }

        // assign the value of p_array[1] to the opposite of the green line last segment
        if (this->is_spiro) {
            Geom::CubicBezier const *cubic = dynamic_cast<Geom::CubicBezier const *>(this->green_curve->last_segment());
            if (cubic) {
                p_array[1] = (*cubic)[3] + (*cubic)[3] - (*cubic)[2];
                ctrl[FRONT_HANDLE]->set_position(p_array[0]);
            } else {
                p_array[1] = p_array[0];
            }
        }

        for (auto &c : ctrl) {
            c->set_visible(false);
        }
        cl0->set_visible(false);
        cl1->set_visible(false);
        this->state = PenTool::POINT;

        if (this->is_polylines_paraxial) {
            // We compare the point we're removing with the nearest horiz/vert to
            // see if the line was added with SHIFT or not.
            Geom::Point compare(pt);
            this->_setToNearestHorizVert(compare, 0);
            if ((std::abs(compare[Geom::X] - pt[Geom::X]) > 1e-9) ||
                (std::abs(compare[Geom::Y] - pt[Geom::Y]) > 1e-9)) {
                this->paraxial_angle = this->paraxial_angle.cw();
            }
        }
        this->_setSubsequentPoint(pt, true);

        // redraw
        this->_bsplineSpiroBuild();
        ret = true;
    }

    return ret;
}

/** Re-add the last undone point to the path being drawn */
bool PenTool::_redoLastPoint()
{
    if (_redo_stack.empty()) {
        return false;
    }

    auto old_green = std::move(_redo_stack.back());
    _redo_stack.pop_back();
    green_curve->set_pathvector(old_green);

    if (auto const *last_seg = green_curve->last_segment()) {
        Geom::Path freshly_added;
        freshly_added.append(*last_seg);
        green_bpaths.emplace_back(make_canvasitem<CanvasItemBpath>(_desktop->getCanvasSketch(), freshly_added, true));
    }
    green_bpaths.back()->set_stroke(green_color);
    green_bpaths.back()->set_fill(0x0, SP_WIND_RULE_NONZERO);

    auto const last_point = green_curve->last_point();
    if (last_point) {
        p_array[0] = p_array[1] = *last_point;
    }
    _setSubsequentPoint(p_array[3], true);
    _bsplineSpiroBuild();

    _did_redo = true;
    return true;
}

void PenTool::_finish(gboolean const closed)
{
    if (this->expecting_clicks_for_LPE > 1) {
        // don't let the path be finished before we have collected the required number of mouse clicks
        return;
    }

    this->_disableEvents();

    this->message_context->clear();

    _desktop->messageStack()->flash(Inkscape::NORMAL_MESSAGE, _("Drawing finished"));

    // cancelate line without a created segment
    this->red_curve.reset();
    spdc_concat_colors_and_flush(this, closed);
    this->sa = nullptr;
    this->ea = nullptr;

    this->npoints = 0;
    this->state = PenTool::POINT;

    for (auto &c : ctrl) {
        c->set_visible(false);
    }

    cl0->set_visible(false);
    cl1->set_visible(false);

    anchors.clear();

    // hide the anchors
    fh_anchor->ctrl->set_visible(false);
    bh_anchor->ctrl->set_visible(false);

    drag_handle_statusbar = false;
    node_mode_statusbar = false;

    this->green_anchor.reset();
    _redo_stack.clear();
    this->_enableEvents();
}

void PenTool::_disableEvents()
{
    this->events_disabled = true;
}

void PenTool::_enableEvents()
{
    g_return_if_fail(this->events_disabled != 0);

    this->events_disabled = false;
}

void PenTool::waitForLPEMouseClicks(Inkscape::LivePathEffect::EffectType effect_type, unsigned int num_clicks,
                                    bool use_polylines)
{
    if (effect_type == Inkscape::LivePathEffect::INVALID_LPE)
        return;

    this->waiting_LPE_type = effect_type;
    this->expecting_clicks_for_LPE = num_clicks;
    this->is_polylines_only = use_polylines;
    this->is_polylines_paraxial = false; // TODO: think if this is correct for all cases
}

void PenTool::nextParaxialDirection(Geom::Point const &pt, Geom::Point const &origin, guint state)
{
    //
    // after the first mouse click we determine whether the mouse pointer is closest to a
    // horizontal or vertical segment; for all subsequent mouse clicks, we use the direction
    // orthogonal to the last one; pressing Shift toggles the direction
    //
    // num_clicks is not reliable because spdc_pen_finish_segment is sometimes called too early
    // (on first mouse release), in which case num_clicks immediately becomes 1.
    // if (this->num_clicks == 0) {

    if (this->green_curve->is_unset()) {
        // first mouse click
        double h = pt[Geom::X] - origin[Geom::X];
        double v = pt[Geom::Y] - origin[Geom::Y];
        this->paraxial_angle = Geom::Point(h, v).ccw();
    }
    if (!(state & GDK_SHIFT_MASK)) {
        this->paraxial_angle = this->paraxial_angle.ccw();
    }
}

void PenTool::_setToNearestHorizVert(Geom::Point &pt, guint const state) const
{
    Geom::Point const origin = p_array[0];
    Geom::Point const target = (state & GDK_SHIFT_MASK) ? this->paraxial_angle : this->paraxial_angle.ccw();

    // Create a horizontal or vertical constraint line
    Inkscape::Snapper::SnapConstraint cl(origin, target);

    // Snap along the constraint line; if we didn't snap then still the constraint will be applied
    SnapManager &m = _desktop->getNamedView()->snap_manager;

    Inkscape::Selection *selection = _desktop->getSelection();
    // selection->singleItem() is the item that is currently being drawn. This item will not be snapped to (to avoid
    // self-snapping)
    // TODO: Allow snapping to the stationary parts of the item, and only ignore the last segment

    m.setup(_desktop, true, selection->singleItem());
    m.constrainedSnapReturnByRef(pt, Inkscape::SNAPSOURCE_NODE_HANDLE, cl);
    m.unSetup();
}

} // namespace Tools
} // namespace UI
} // namespace Inkscape

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
