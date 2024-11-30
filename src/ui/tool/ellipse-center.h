// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Control point at the center of an elliptical arc in the Node Tool.
 */
/* Authors:
 *   Rafał M. Siejakowski <rs@rs-math.net>
 *   Adam Belis <adam.belis@gmail.com>
 *
 * Copyright (C) 2024 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_TOOL_ELLIPSE_CENTER_H
#define INKSCAPE_UI_TOOL_ELLIPSE_CENTER_H

#include <2geom/elliptical-arc.h>
#include <optional>

#include "ui/tool/control-point.h"

class SPItem;

namespace Geom {
class Point;
}

namespace Inkscape {
struct MotionEvent;

namespace UI {
class EllipticalManipulator;
class NodeSharedData;

class EllipseCenter : public ControlPoint
{
public:
    EllipseCenter(SPDesktop &desktop, NodeSharedData const &data, Geom::Point const &pos,
                  EllipticalManipulator &manipulator, SPItem const *path);

    ~EllipseCenter() override = default;

    void move(Geom::Point const &pos) final;

protected:
    Glib::ustring _getTip(unsigned) const final;

    void dragged(Geom::Point &new_pos, MotionEvent const &event) final;

    bool grabbed(MotionEvent const & /* event */) final;

    void ungrabbed(ButtonReleaseEvent const * /* event */) final;

private:
    EllipticalManipulator *_manipulator = nullptr;
    SPItem const *_path = nullptr;
    std::optional<Geom::EllipticalArc> _arc_at_drag_start;
};
} // namespace UI
} // namespace Inkscape

#endif // INKSCAPE_UI_TOOL_ELLIPSE_CENTER_H

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
