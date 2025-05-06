// SPDX-License-Identifier: GPL-2.0-or-later
/* Authors:
 *   Rafał M. Siejakowski <rs@rs-math.net>
 *
 * Copyright (C) 2024 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_TOOL_BEZIER_CURVE_HANDLER_H
#define INKSCAPE_UI_TOOL_BEZIER_CURVE_HANDLER_H

#include "ui/tool/curve-event-handler.h"

namespace Inkscape::UI {

class BezierCurveHandler : public CurveHandler
{
public:
    explicit BezierCurveHandler(bool is_bspline)
        : _is_bspline{is_bspline}
    {}

    ~BezierCurveHandler() override = default;

    bool pointGrabbed(NodeList::iterator curve_start, NodeList::iterator curve_end) override;

    bool pointDragged(NodeList::iterator curve_start, NodeList::iterator curve_end, double curve_time,
                      Geom::Point const &drag_origin, Geom::Point const &drag_destination,
                      MotionEvent const &event) override;

    Glib::ustring getTooltip(unsigned event_state, NodeList::iterator curve_start) override;

private:
    bool _is_bspline = false;
    bool _segment_degenerate_at_drag_start = false;
};
} // namespace Inkscape::UI

#endif // INKSCAPE_UI_TOOL_BEZIER_CURVE_HANDLER_H

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
