// SPDX-License-Identifier: GPL-2.0-or-later
/* Authors:
 *   Krzysztof Kosiński <tweenk.pl@gmail.com>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Rafał M. Siejakowski <rs@rs-math.net>
 *
 * Copyright (C) 2009-2024 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "ui/tool/bezier-curve-handler.h"

#include <2geom/math-utils.h>
#include <glib/gi18n.h>

#include "ui/tool/control-point.h"
#include "ui/tool/node.h"
#include "ui/widget/events/canvas-event.h"

namespace {

/** Compute the "weight" describing how the influence of the drag should be distributed
 *  between the handles; 0 = front handle only, 1 = back handle only.
 */
double compute_bezier_drag_weight_for_time(double t)
{
    if (t <= 1.0 / 6.0) {
        return 0.0;
    }
    if (t <= 0.5) {
        return Geom::cube((6 * t - 1) / 2.0) / 2;
    }
    if (t <= 5.0 / 6.0) {
        return (1 - Geom::cube((6 * (1 - t) - 1) / 2.0)) / 2 + 0.5;
    }
    return 1.0;
}
} // namespace

namespace Inkscape::UI {

bool BezierCurveHandler::pointGrabbed(NodeList::iterator curve_start, NodeList::iterator curve_end)
{
    // move the handles to 1/3 the length of the segment for line segments
    auto *initial_handle = curve_start->front();
    auto *final_handle = curve_end->back();

    if (initial_handle->isDegenerate() && final_handle->isDegenerate()) {
        _segment_degenerate_at_drag_start = true;

        // delta is a vector equal 1/3 of the way between endpoint nodes
        Geom::Point const delta = (curve_end->position() - curve_start->position()) / 3.0;

        if (!_is_bspline) {
            initial_handle->move(initial_handle->position() + delta);
            final_handle->move(final_handle->position() - delta);
            return true;
        }
        return false;
    }

    _segment_degenerate_at_drag_start = false;
    return false;
}

bool BezierCurveHandler::pointDragged(NodeList::iterator curve_start, NodeList::iterator curve_end, double curve_time,
                                      Geom::Point const &drag_origin, Geom::Point const &drag_destination,
                                      MotionEvent const &event)
{
    // special cancel handling - retract handles when if the segment was degenerate
    if (ControlPoint::is_drag_cancelled(event) && _segment_degenerate_at_drag_start) {
        curve_start->front()->retract();
        curve_end->back()->retract();
        return true;
    }

    double const &t = curve_time;
    double const weight = compute_bezier_drag_weight_for_time(t);
    auto const delta = drag_destination - drag_origin;

    auto *initial_handle = curve_start->front();
    auto *final_handle = curve_end->back();

    // Magic Bezier Drag Equations follow!
    if (!_is_bspline) {
        auto const offset0 = ((1 - weight) / (3 * t * (1 - t) * (1 - t))) * delta;
        auto const offset1 = (weight / (3 * t * t * (1 - t))) * delta;

        initial_handle->move(initial_handle->position() + offset0);
        final_handle->move(final_handle->position() + offset1);
        return true;
    }

    if (weight >= 0.8) {
        if (held_shift(event)) {
            final_handle->move(drag_destination);
        } else {
            curve_end->move(curve_end->position() + delta);
        }
    } else if (weight <= 0.2) {
        if (held_shift(event)) {
            initial_handle->move(drag_destination);
        } else {
            curve_start->move(curve_start->position() + delta);
        }
    } else {
        curve_start->move(curve_start->position() + delta);
        curve_end->move(curve_end->position() + delta);
    }
    return true;
}

Glib::ustring BezierCurveHandler::getTooltip(unsigned event_state, NodeList::iterator curve_start)
{
    if (state_held_shift(event_state) && _is_bspline) {
        return C_("Path segment tip", "<b>Shift</b>: drag to open or move BSpline handles");
    }
    if (state_held_shift(event_state)) {
        return C_("Path segment tip", "<b>Shift</b>: click to toggle segment selection");
    }
    if (state_held_ctrl(event_state) && state_held_alt(event_state)) {
        return C_("Path segment tip", "<b>Ctrl+Alt</b>: click to insert a node");
    }
    if (state_held_alt(event_state)) {
        return C_("Path segment tip", "<b>Alt</b>: double click to change line type");
    }
    if (_is_bspline) {
        return C_("Path segment tip", "<b>BSpline segment</b>: drag to shape the segment, doubleclick to insert node, "
                                      "click to select (more: Alt, Shift, Ctrl+Alt)");
    }

    bool const linear = curve_start->front()->isDegenerate() && curve_start.next()->back()->isDegenerate();
    if (linear) {
        return C_("Path segment tip", "<b>Linear segment</b>: drag to convert to a Bezier segment, "
                                      "doubleclick to insert node, click to select (more: Alt, Shift, Ctrl+Alt)");
    }

    return C_("Path segment tip", "<b>Bezier segment</b>: drag to shape the segment, doubleclick to insert node, "
                                  "click to select (more: Alt, Shift, Ctrl+Alt)");
}
} // namespace Inkscape::UI

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
