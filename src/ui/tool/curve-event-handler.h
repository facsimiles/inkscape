// SPDX-License-Identifier: GPL-2.0-or-later
/* Authors:
 *   Rafał M. Siejakowski <rs@rs-math.net>
 *
 * Copyright (C) 2024 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_TOOL_CURVE_EVENT_HANDLER_H
#define INKSCAPE_UI_TOOL_CURVE_EVENT_HANDLER_H

#include <glibmm/ustring.h>

#include "ui/tool/node.h"

namespace Geom {
class Point;
}

namespace Inkscape {
class MotionEvent;

namespace UI {

/**
 * An interface handling the UI actions performed on a point on the curve ("dragpoint")
 * in the Node Tool. Dragging or clicking a point on the curve (between the nodes) may
 * have a different result than performing these operations on a node. Furthermore, the
 * result of these actions may depend on the type of curve segment.
 */
struct CurveHandler
{
    /**
     * Handle the initial grab of a point on the curve.
     *
     * @param curve_start  Start node of the grabbed segment.
     * @param curve_end    End node of the grabbed segment.
     *
     * @return Whether the geometry needs to be updated.
     */
    virtual bool pointGrabbed(NodeList::iterator curve_start, NodeList::iterator curve_end) = 0;

    /**
     * Handle an ongoing drag of a point on a curve segment.
     *
     * @param curve_start       Initial node of the curve segment.
     * @param curve_end         Final node of the curve segment.
     * @param curve_time        Time parameter of the dragged point, in the interval [0, 1].
     * @param drag_origin       The point from which the drag started.
     * @param drag_destination  The point to which the curve is being dragged.
     * @param event             The details of the motion event.
     *
     * @return Whether the curve geometry should be updated.
     */
    virtual bool pointDragged(NodeList::iterator curve_start, NodeList::iterator curve_end, double curve_time,
                              Geom::Point const &drag_origin, Geom::Point const &drag_destination,
                              MotionEvent const &event) = 0;

    /// Get the tooltip string with Pango markup, to be displayed on the status bar.
    virtual Glib::ustring getTooltip(unsigned event_state, NodeList::iterator curve_start) = 0;

    virtual ~CurveHandler() = default;
};
} // namespace UI
} // namespace Inkscape

#endif // INKSCAPE_UI_TOOL_CURVE_DRAG_HANDLER_H

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
