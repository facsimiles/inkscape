// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Connector creation tool
 *
 * Authors:
 *   Martin Owens <doctormo@gmail.com>
 *
 * Copyright (C) 2023 Martin Owens
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 *
 */

#include "ui/tools/connector-tool.h"

#include <cstring>
#include <gdk/gdkkeysyms.h>
#include <string>

#include "desktop-style.h"
#include "display/curve.h"
#include "layer-manager.h"
#include "live_effects/effect.h"
#include "live_effects/lpeobject.h"
#include "object/sp-root.h"
#include "svg/svg.h"
#include "ui/modifiers.h"
#include "ui/shape-editor.h"
#include "ui/toolbar/connector-toolbar.h"
#include "ui/tools/connector-tool-knotholders.h"
#include "ui/widget/events/canvas-event.h"

using namespace Inkscape::LivePathEffect;
using Inkscape::Modifiers::Modifier;

namespace Inkscape::UI::Tools {

LPEConnectorLine *ConnectorTool::getLPE(SPShape *line)
{
    return dynamic_cast<LPEConnectorLine *>(line->getCurrentLPE());
}

ConnectorTool::ConnectorTool(SPDesktop *desktop)
    : ToolBase(desktop, "/tools/connector", "connector.svg")
{
    sp_event_context_read(this, "curvature");
    sp_event_context_read(this, "orthogonal");
    sp_event_context_read(this, "jump-size");
    sp_event_context_read(this, "jump-type");
    sp_event_context_read(this, "spacing");
    sp_event_context_read(this, "steps");

    shape_editor = new ShapeEditor(desktop);

    drawn_line = make_canvasitem<CanvasItemBpath>(desktop->getCanvasSketch());
    drawn_line->set_stroke(0x0000ff7f); // blue
    drawn_line->set_fill(0x0, SP_WIND_RULE_NONZERO);
    drawn_line->set_visible(false);

    // We'd like to be able to make the stroke line fatter, but canvas-item-rect is fixed width atm
    highlight_a = make_canvasitem<CanvasItemRect>(desktop->getCanvasSketch());
    highlight_a->set_stroke(0x00ff007f); // green
    highlight_a->set_stroke_width(3);
    highlight_a->set_fill(0x0);
    highlight_a->set_visible(false);

    highlight_b = make_canvasitem<CanvasItemRect>(desktop->getCanvasSketch());
    highlight_b->set_stroke(0x0066667f); // cyan
    highlight_b->set_stroke_width(3);
    highlight_b->set_fill(0x0);
    highlight_b->set_visible(false);

    point_editing_rect = make_canvasitem<CanvasItemRect>(desktop->getCanvasSketch());
    point_editing_rect->set_stroke(0xff33337f);
    point_editing_rect->set_fill(0x0);
    point_editing_rect->set_dashed(true);
    point_editing_rect->set_visible(false);

    tool_mode = CONNECTOR_LINE_MODE;
    toolbar = dynamic_cast<Toolbar::ConnectorToolbar *>(desktop->get_toolbar_by_name("ConnectorToolbar"));
    toolbar->_line_tool.set_active(true);

    sel_changed_connection =
        desktop->getSelection()->connectChanged(sigc::mem_fun(*this, &ConnectorTool::selectionChanged));
    selectionChange();
}

ConnectorTool::~ConnectorTool()
{
    if (hover_knots) {
        delete hover_knots;
        hover_knots = nullptr;
    }

    selectionClear();
    deactivateLineDrawing();
    last_route = Geom::PathVector();
    drawn_line = nullptr;
    highlight_a = nullptr;
    highlight_b = nullptr;
    point_editing_rect = nullptr;
}

void ConnectorTool::set(Inkscape::Preferences::Entry const &val)
{
    std::string select_name = val.getEntryName();
    std::string select_value = val.getString();

    if (val.getEntryName() == "curvature") {
        curvature = val.getDoubleLimited();
    } else if (val.getEntryName() == "orthogonal") {
        conn_type = val.getBool() ? Avoid::ConnType_Orthogonal : Avoid::ConnType_PolyLine;
        select_name = "line-type";
        select_value = ConnectorType.get_key(conn_type);
    } else if (val.getEntryName() == "jump-size") {
        jump_size = val.getDoubleLimited();
    } else if (val.getEntryName() == "jump-type") {
        if (val.getBool()) {
            jump_type = JumpMode::Arc;
            select_value = "arc";
        } else {
            jump_type = JumpMode::Gap;
            select_value = "gap";
        }
    } else if (val.getEntryName() == "spacing") {
        spacing = val.getDoubleLimited();
    } else if (val.getEntryName() == "steps") {
        steps = val.getUInt();
        select_name = "";
    }
    // Update selected lines.
    if (!select_name.empty()) {
        bool modified = false;
        for (auto &line : selected_lines) {
            auto repr = getLPE(line)->getRepr();
            auto attr = repr->attribute(select_name.c_str());
            if ((!attr) || select_value != attr) {
                modified = true;
                getLPE(line)->getRepr()->setAttribute(select_name, select_value);
            }
        }
        if (modified) {
            auto document = _desktop->getDocument();
            DocumentUndo::maybeDone(document, "connect-setting", _("Change connector setting"), "");
        }
    }
}

/**
 * Finds the shape at the given point, excluding Connector Lines.
 */
SPItem *ConnectorTool::findShapeAtPoint(Geom::Point w_point, bool ignore_lines)
{
    for (auto child : _desktop->getDocument()->getItemsAtPoints(_desktop->dkey, {w_point}, false, false)) {
        if (!is<SPPoint>(child) && (!ignore_lines || !isConnector(child))) {
            return child;
        }
    }
    return nullptr;
}

/**
 * Like findShapeAtPoint, but includes a distance from the point.
 */
SPItem *ConnectorTool::findShapeNearPoint(Geom::Point dt_point, int distance)
{
    auto box = Geom::Rect(dt_point, dt_point);
    box.expandBy(distance);
    for (auto &child : _desktop->getDocument()->getItemsPartiallyInBox(_desktop->dkey, box)) {
        return child;
    }
    return nullptr;
}

bool ConnectorTool::root_handler(CanvasEvent const &event)
{
    bool const line_mode = tool_mode == CONNECTOR_LINE_MODE;
    bool const point_mode = tool_mode == CONNECTOR_POINT_MODE;

    bool ret = false;

    inspect_event(event,
        [&] (ButtonPressEvent const &event) {
            if (event.num_press == 1 && event.button == 1) {
                if (line_mode) {
                    ret = activateHoverPoint();
                }
            } else if (event.num_press == 2) {
                if (line_mode) {
                    // Attempt to add a new checkpoint with double click
                    for (auto &line : selected_lines) {
                        auto affine = line->i2dt_affine().inverse();
                        if (addCheckpoint(line, _desktop->w2d(event.pos) * affine)) {
                            ret = true;
                            selectionChange();
                            break;
                        }
                    }
                    // Double click non-line object selects it.
                    if (!ret) {
                        if (auto item = findShapeAtPoint(event.pos)) {
                            _desktop->getSelection()->set(item);
                            deactivateLineDrawing();
                            ret = true;
                        }
                    }
                }
                auto item = _desktop->getSelection()->singleItem();
                if (point_mode && item) {
                    auto affine = item->i2dt_affine().inverse();
                    auto point = new Geom::Point(_desktop->w2d(event.pos) * affine);
                    if (SPPoint::makePointAbsolute(item, point)) {
                        selectionChange();
                    }
                    ret = true;
                }

            }
        },
        [&] (ButtonReleaseEvent const &event) {
            if (event.button == 1) {
                if (point_mode || !hover_item) {
                    if (auto item = findShapeNearPoint(_desktop->w2d(event.pos), 6.0)) {
                        if (Modifier::get(Modifiers::Type::SELECT_ADD_TO)->active(event.modifiers)) {
                            _desktop->getSelection()->add(item);
                        } else {
                            _desktop->getSelection()->set(item);
                        }
                    }
                    ret = true;
                } else if (line_mode) {
                    ret = activateHoverPoint();
                }
            }
        },
        [&] (MotionEvent const &event) {
            // Ignore middle-button scrolling
            if (!(event.modifiers & (GDK_BUTTON2_MASK | GDK_BUTTON3_MASK))) {
                auto point_dt = _desktop->w2d(event.pos);
                auto point_doc = _desktop->dt2doc(point_dt);

                auto const item = findShapeAtPoint(event.pos, true);

                /// This removes the knotholder when item is nullptr too
                if (hover_knots && hover_knots->getItem() != item) {
                    // We're going to check that the mouse point is within the bounding box
                    // for the old item, we don't change objects unless we stray outside
                    auto bbox_doc = hover_knots->getItem()->documentVisualBounds();
                    // Expand the bounding box to make it a little easier to hit nodes right on the edge
                    if (bbox_doc) {
                        bbox_doc->expandBy(3.0);
                    }
                    // Either we are over a new object (inside the old one) or we don't want to unhover.
                    if (item || !(bbox_doc && bbox_doc->contains(point_doc))) {
                        delete hover_knots;
                        hover_knots = nullptr;
                        unhighlightPoint();
                    }
                }
                // Conversely only create one when the item is defined
                if (line_mode && !hover_knots && item) {
                    highlightPoint(item);
                    hover_knots = new ConnectorPointsKnotHolder(_desktop, item);
                }
                if ((drawn_start || modify_line) && !hover_item) {
                    // Free flowing line, usually not needed.
                    moveDrawnLine(&point_dt);
                }
                // Update cursor
                if (active_item || hover_item || modify_line) {
                    set_cursor("connector.svg");
                } else {
                    set_cursor("select.svg");
                }
            }
        },
        [&] (KeyPressEvent const &event) {
            switch (get_latin_keyval(event)) {
                case GDK_KEY_Escape:
                    if (drawn_start || modify_line) {
                        deactivateLineDrawing();
                        ret = true;
                    }
                    break;
                default:
                    break;
            }
        },
        [&] (CanvasEvent const &event) {}
    );

    return ret || ToolBase::root_handler(event);
}

/**
 * Deactivate any ongoing connection.
 */
void ConnectorTool::deactivateLineDrawing()
{
    // when drawing a new line, start point
    active_item = nullptr;
    active_point = nullptr;
    active_hint_name = "";
    active_hint_point = nullptr;

    // Hovering item for either new or reconnection
    hover_item = nullptr;
    hover_point = nullptr;
    hover_hint_name = "";
    hover_hint_point = nullptr;

    // When modifying a line connection
    modify_line = nullptr;
    modify_end = false;

    // Visual elements
    drawn_start.reset();
    drawn_line->set_visible(false);
    highlight_a->set_visible(false);
    highlight_b->set_visible(false);
    point_editing_rect->set_visible(false);
}

/**
 * Highlight the whole bounding box of the item or point, plus a few pixels.
 *
 * @param rect - The canvas rectangle to reshape
 * @param item - The parent item the point belongs to
 * @param point - (optional) The relative position in the parent of the highlight.
 */
void ConnectorTool::setHighlightArea(CanvasItemPtr<CanvasItemRect> &rect, SPItem *item, Geom::Point *point)
{
    g_assert(rect);
    g_assert(item);

    rect->set_visible(false);
    auto bounds = item->desktopVisualBounds();
    if (!bounds) {
        return;
    }

    // Default is highlight whole object
    Geom::Rect box = *bounds;

    // But then if we have a point to make, highlight that instead.
    if (point) {
        auto item_point = SPPoint::getItemPoint(item, point);
        if (!item_point) {
            return; // Show nothing.
        }

        // Adjust item units to _desktop units so they appear in the right places.
        auto dt_point = *item_point * item->i2dt_affine();
        box = Geom::Rect(dt_point, dt_point);
        box.expandBy(4.0); // Must be even
    }

    // Grow end box slightly.
    box.expandBy(3.0);
    rect->set_rect(box);
    rect->set_visible(true);
}

/**
 * Called to re-draw the in-progress line with point being the furthest element.
 */
void ConnectorTool::moveDrawnLine(Geom::Point *drawn_end, Geom::Point *hover_sub_point)
{
    if (modify_line) {
        return moveReconnectLine(drawn_end, hover_sub_point);
    }

    if (!drawn_start || !drawn_end || *drawn_end == *drawn_start) {
        return;
    }

    auto router = _desktop->getDocument()->getRouter();
    auto document = _desktop->getDocument();
    auto i2dt = document->getRoot()->i2dt_affine();

    // Put _desktop path into document for routing
    auto pathv = drawSimpleLine(*drawn_end) * i2dt.inverse();

    // Get a sub point from whatever is active to feed to drawn router
    Geom::Point *active_sub_point = nullptr;
    if (active_item) {
        if (active_point) {
            active_sub_point = active_point->parentPoint();
        } else if (active_hint_point) {
            active_sub_point = active_hint_point;
        } else {
            active_sub_point = new Geom::Point(0.5, 0.5);
        }
    }

    // Create a routed line so the user can see what this line would look like
    // Even though start and end are correct, the routing depends on the bounding box
    // so we still need to feed in the prospective objects we're connecting to.
    last_route =
        LPEConnectorLine::generate_path(pathv, router, document->getRoot(), active_item, active_sub_point, hover_item,
                                        hover_sub_point, conn_type, curvature);

    // Now routng is done, put new path back into _desktop path for display
    drawn_line->set_bpath(last_route * i2dt, true);
    drawn_line->set_visible(true);
}

void ConnectorTool::moveReconnectLine(Geom::Point *point, Geom::Point *hover_sub_point)
{
    auto document = _desktop->getDocument();
    auto router = document->getRouter();

    // Get original pathv from item.
    auto pathv = modify_line->curveForEdit()->get_pathvector();
    auto i2dt = modify_line->i2dt_affine();

    auto lpe_effect = getLPE(modify_line);
    auto static_item = modify_end ? lpe_effect->getConnStart() : lpe_effect->getConnEnd();
    Geom::Point *static_sub_point = nullptr;

    if (static_item) {
        if (auto static_point = cast<SPPoint>(static_item)) {
            static_sub_point = static_point->parentPoint();
            static_item = cast<SPItem>(static_point->parent);
        } else {
            static_sub_point = new Geom::Point(0.5, 0.5);
        }
    }

    // Reposition one end of the path
    // XXX We may want to reset the initial and final curves to remove directionality.
    if (modify_end) {
        pathv[0].setFinal(*point * i2dt.inverse());
        pathv = LPEConnectorLine::generate_path(pathv, router, document->getRoot(), static_item, static_sub_point,
                                                hover_item, hover_sub_point, lpe_effect->getConnType(),
                                                lpe_effect->getCurvature());

    } else {
        pathv[0].setInitial(*point * i2dt.inverse());
        pathv = LPEConnectorLine::generate_path(pathv, router, document->getRoot(), hover_item, hover_sub_point,
                                                static_item, static_sub_point, lpe_effect->getConnType(),
                                                lpe_effect->getCurvature());
    }

    // Don't store pathv, it's not a clean route.
    last_route = Geom::PathVector();
    // Show the routed path on the screen for the user to review as they draw.
    drawn_line->set_bpath(pathv * i2dt, true);
    drawn_line->set_visible(true);
}

/**
 * Calculate a simple vector between the active start and the given end.
 */
Geom::PathVector ConnectorTool::drawSimpleLine(Geom::Point drawn_end)
{
    g_assert(drawn_start);
    // Draw a basic three part line.
    Geom::PathVector pathv;
    pathv.push_back(Geom::Path(*drawn_start));
    for (int i = 0; i < steps; ++i) {
        double time = (1.0 / (steps + 1)) * (i + 1);
        pathv.back().appendNew<Geom::LineSegment>(Geom::lerp(time, *drawn_start, drawn_end));
    }
    pathv.back().appendNew<Geom::LineSegment>(drawn_end);
    return pathv;
}

/**
 * Called when we're completing the drawing of the line.
 */
void ConnectorTool::finishDrawnLine(SPItem *end_item, SPPoint *end_point)
{
    if (modify_line) {
        return finishReconnectLine(end_item, end_point);
    }

    auto document = _desktop->getDocument();

    // If the active point is virtual, create it.
    if (active_hint_point) {
        active_point = SPPoint::makePointRelative(active_item, active_hint_point, active_hint_name);
    }

    // Set start point to itself if no SPPoint selected.
    SPItem *conn_start = active_item;
    if (active_point) {
        conn_start = active_point;
    }
    // Start point should already be calculated, just need to calculate the end point
    SPItem *conn_end = end_item;
    Geom::Point *end_parent_point = new Geom::Point(0.5, 0.5);
    if (end_point) {
        conn_end = end_point;
        end_parent_point = end_point->parentPoint();
    }
    if (conn_start == conn_end) {
        // Refuse to make a connection between itself
        return;
    }

    auto pt_point = SPPoint::getItemPoint(end_item, end_parent_point);

    Inkscape::XML::Node *repr = document->getReprDoc()->createElement("svg:path");
    sp_desktop_apply_style_tool(_desktop, repr, "/tools/connector", false);

    // Add it so we can adjust the affine
    auto new_line = cast<SPItem>(_desktop->layerManager().currentLayer()->appendChildRepr(repr));

    // Start is in _desktop units, so convert for easier calculation.
    auto pathv = drawSimpleLine(*pt_point * end_item->i2dt_affine()) * new_line->i2dt_affine().inverse();

    // Add in any directionality and automatic adjustment instructions
    if (!last_route.empty()) {
        for (int i = 1; i < pathv[0].size(); i++) {
            auto dir = LPEConnectorLine::detectCheckpointOrientation(last_route[0], pathv[0].nodes()[i]);

            int dynamic = DynamicNone;
            if (dir & Avoid::ConnDirVert) {
                dynamic += DynamicY;
            }
            if (dir & Avoid::ConnDirHorz) {
                dynamic += DynamicX;
            }
            pathv = LPEConnectorLine::rewriteLine(pathv[0], i, pathv[0].nodes()[i], dir, dynamic);
        }
    }

    repr->setAttribute("d", sp_svg_write_path(pathv));
    Inkscape::GC::release(repr);

    auto lpe_repr = LivePathEffect::Effect::createEffect("connector_line", document);
    LivePathEffect::Effect::applyEffect(lpe_repr, new_line);
    lpe_repr->setAttribute("connection-start", g_strdup_printf("#%s", conn_start->getId()));
    lpe_repr->setAttribute("connection-end", g_strdup_printf("#%s", conn_end->getId()));
    lpe_repr->setAttribute("line-type", ConnectorType.get_key(conn_type).c_str());
    lpe_repr->setAttribute("jump-type", jump_type == JumpMode::Arc ? "arc" : "gap");
    lpe_repr->setAttributeSvgDouble("jump-size", jump_size);
    lpe_repr->setAttributeSvgDouble("spacing", spacing);
    lpe_repr->setAttributeSvgDouble("curvature", curvature);
    Inkscape::GC::release(lpe_repr);

    deactivateLineDrawing();
    DocumentUndo::maybeDone(document, "connect-line", _("Draw connector line"), "");
    _desktop->getSelection()->set(new_line);
}

void ConnectorTool::finishReconnectLine(SPItem *end_item, SPPoint *end_point)
{
    auto document = _desktop->getDocument();
    auto lpe_repr = getLPE(modify_line)->getRepr();

    auto conn = end_point ? end_point : end_item;
    if (modify_end) {
        lpe_repr->setAttribute("connection-end", g_strdup_printf("#%s", conn->getId()));
    } else {
        lpe_repr->setAttribute("connection-start", g_strdup_printf("#%s", conn->getId()));
    }

    deactivateLineDrawing();
    DocumentUndo::maybeDone(document, "reconnect-line", _("Reconnect line"), "");
    selectionChange();
}

/**
 * Highlight the given item or sub-point with a box.
 */
void ConnectorTool::highlightPoint(SPItem *item, SPPoint *sp_point)
{
    // This produces a snapping like effect in the drawn line.
    auto parent_point = sp_point ? sp_point->parentPoint() : new Geom::Point(0.5, 0.5);

    hover_item = item;
    hover_point = sp_point;
    hover_hint_name = "";
    hover_hint_point = nullptr;

    setHighlightArea(highlight_b, item, sp_point ? parent_point : nullptr);
    if (active_item || modify_line) {
        auto point = SPPoint::getItemPoint(hover_item, parent_point);
        auto drawn_end = new Geom::Point(*point * hover_item->i2dt_affine());
        moveDrawnLine(drawn_end, parent_point);
    }
}

void ConnectorTool::highlightPoint(SPItem *item, std::string name, Geom::Point parent_point)
{
    hover_item = item;
    hover_point = nullptr;
    hover_hint_name = name;
    hover_hint_point = new Geom::Point(parent_point);

    setHighlightArea(highlight_b, item, &parent_point);
    if (active_item || modify_line) {
        auto point = SPPoint::getItemPoint(hover_item, hover_hint_point);
        auto drawn_end = new Geom::Point(*point * hover_item->i2dt_affine());
        moveDrawnLine(drawn_end, hover_hint_point);
    }
}

void ConnectorTool::unhighlightPoint()
{
    highlight_b->set_visible(false);
    hover_item = nullptr;
}

/**
 * Activate (start or end line) at the current hover location.
 */
bool ConnectorTool::activateHoverPoint()
{
    if (hover_item && hover_item != active_item) {
        if (hover_hint_point) {
            activatePoint(hover_item, hover_hint_name, *hover_hint_point);
        } else {
            activatePoint(hover_item, hover_point);
        }
        return true;
    }
    return false;
}

/**
 * Called when an object's sub-node is clicked on (only from knots)
 */
void ConnectorTool::activatePoint(SPItem *item, SPPoint *sp_point)
{
    if (active_item || modify_line) {
        return finishDrawnLine(item, sp_point);
    }

    auto parent_point = sp_point ? sp_point->parentPoint() : new Geom::Point(0.5, 0.5);
    active_item = item;
    active_point = sp_point;
    setHighlightArea(highlight_a, item, parent_point);
    auto point = SPPoint::getItemPoint(item, parent_point);
    drawn_start = *point * active_item->i2dt_affine();;
}

/**
 * Called when an object's virtual sub-node is clicked on (only from knot)
 */
void ConnectorTool::activatePoint(SPItem *item, std::string name, Geom::Point parent_point)
{
    if (active_item || modify_line) {
        // Create new hint point and finish the drawn line with it.
        return finishDrawnLine(item, SPPoint::makePointRelative(item, &parent_point, name));
    }

    active_item = item;
    active_hint_name = name;
    active_hint_point = new Geom::Point(parent_point);
    setHighlightArea(highlight_a, item, &parent_point);
    auto point = SPPoint::getItemPoint(item, &parent_point);
    drawn_start = *point * active_item->i2dt_affine();;
}

/**
 * Active the line-end for reconnection, see moveReconnectLine.
 */
void ConnectorTool::activateLine(SPShape *line, bool is_end)
{
    // deactivate ensures we're not doing something else.
    deactivateLineDrawing();
    modify_end = is_end;
    modify_line = line;
}

/**
 * Clear the selection and reset all vars.
 */
void ConnectorTool::selectionClear()
{
    if (point_editing_holder) {
        delete point_editing_holder;
        point_editing_holder = nullptr;
        point_editing_rect->set_visible(false);
    }
    for (auto line_knots : selected_line_holders) {
        delete line_knots;
    }
    selected_line_holders.clear();
    selected_points.clear();
    selected_lines.clear();
}

void ConnectorTool::setToolMode(int mode)
{
    tool_mode = mode;
    deactivateLineDrawing();
    selectionChange();
}

void ConnectorTool::selectionChange()
{
    selectionChanged(_desktop->getSelection());
}

/**
 * Set the selected items, selecting connected lines where possible.
 */
void ConnectorTool::selectionChanged(Inkscape::Selection *selection)
{
    selectionClear();
    for (auto item : selection->items()) {
        selectObject(item);

        for (auto &connected_item : item->hrefList) {
            selectObject(connected_item);
        }

        for (auto &sp_point : item->getConnectionPoints()) {
            for (auto &connected_item : sp_point->hrefList) {
                selectObject(connected_item);
            }
        }
    }
    toolbar->select_lines(selected_lines);
    toolbar->select_avoided(selection);
}

void ConnectorTool::selectObject(SPObject *object)
{
    if (auto lpe = cast<LivePathEffectObject>(object)) {
        if (auto effect = dynamic_cast<LPEConnectorLine *>(lpe->get_lpe())) {
            for (auto &item : effect->getCurrrentLPEItems()) {
                selectObject(item);
            }
        }
    }
    bool is_connection = isConnector(object);
    if (SPItem *item = cast<SPItem>(object)) {
        if (is_connection && tool_mode == CONNECTOR_LINE_MODE) {
            auto lpe_effect = LPEConnectorLine::get(item);
            if (auto start = cast<SPPoint>(lpe_effect->getConnStart())) {
                selected_points.push_back(start);
            }
            if (auto end = cast<SPPoint>(lpe_effect->getConnEnd())) {
                selected_points.push_back(end);
            }
            auto holder = new ConnectorLineKnotHolder(_desktop, item);
            selected_line_holders.push_back(holder);
            selected_lines.push_back(cast<SPShape>(item));
        }
        if (!is_connection && tool_mode == CONNECTOR_POINT_MODE) {
            auto bounds = item->desktopVisualBounds();
            if (!bounds) {
                return;
            }
            if (point_editing_holder) {
                delete point_editing_holder;
            }
            bounds->expandBy(1.0);
            point_editing_rect->set_rect(*bounds);
            point_editing_rect->set_visible(true);
            point_editing_holder = new ConnectorObjectKnotHolder(_desktop, item);
        }
    }
}

bool ConnectorTool::addCheckpoint(SPShape *line, Geom::Point point)
{
    auto lpe = LPEConnectorLine::get(line);

    // Advanced editor doesn't add checkpoints like this.
    if (lpe->advancedEditor()) {
        return false;
    }

    auto orig_pathv = line->curveForEdit()->get_pathvector();
    auto const &route_pathv = lpe->getRoutePath();

    Geom::Coord distance;
    auto pathv_time = route_pathv.nearestTime(point, &distance);

    if (pathv_time && distance < 5.0) {
        int path_index = -1;
        auto line_point = route_pathv.pointAt(*pathv_time);

        // Detect the orientation so it can be maintained.
        auto dir = LPEConnectorLine::detectCheckpointOrientation(route_pathv, line_point);

        for (int i = 0; i < orig_pathv[0].size(); i++) {
            // Map the original point to the routed point so they can be compared.
            Geom::Coord orig_dist;
            auto orig_time = route_pathv.nearestTime(orig_pathv[0][i].initialPoint(), &orig_dist);
            if (orig_time && orig_dist < 1.0 && pathv_time->asPathTime() > orig_time->asPathTime()) {
                path_index = i + 1;
            }
        }
        if (path_index == -1) {
            // This copy shouldn't be needed, but there is something wrong with the very first path
            // segment that doesn't apply to any other segment. This attempts to compensate for it.
            Geom::Coord orig_dist;
            auto orig_time = route_pathv.nearestTime(orig_pathv[0][0].finalPoint(), &orig_dist);
            if (orig_time && orig_dist < 1.0 && pathv_time->asPathTime() < orig_time->asPathTime()) {
                path_index = 1;
            }
        }
        if (path_index > 0) {
            // Insert the new checkpoint with the given direction detected.
            LPEConnectorLine::rewriteLine(line, path_index, line_point, dir, DynamicNone, RewriteMode::Add);
            DocumentUndo::done(_desktop->getDocument(), _("Add connector checkpoint"), "");
            return true;
        }
    }
    return false;
}

} // namespace Inkscape::UI::Tools

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
