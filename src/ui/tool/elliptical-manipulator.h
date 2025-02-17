// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Handles for the manipulation of elliptical arcs in the Node tool.
 */
/* Authors:
 *   Rafał M. Siejakowski <rs@rs-math.net>
 *
 * Copyright (C) 2024 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_TOOL_ELLIPTICAL_MANIPULATOR_H
#define INKSCAPE_UI_TOOL_ELLIPTICAL_MANIPULATOR_H

#include <2geom/elliptical-arc.h>
#include <2geom/point.h>
#include <glib/gi18n.h>
#include <glibmm/ustring.h>

#include "display/control/canvas-item-bpath.h"
#include "display/control/canvas-item-ptr.h"
#include "ui/tool/commit-events.h"
#include "ui/tool/control-point.h"
#include "ui/tool/node-types.h"

class SPDesktop;
class SPItem;

namespace Geom {
class Ellipse;
class EllipticalArc;
class PathSink;
} // namespace Geom

namespace Inkscape::UI {
class Node;
class NodeSharedData;
class PathManipulator;

class EllipticalManipulator
{
public:
    EllipticalManipulator(SPDesktop &desktop, Geom::EllipticalArc const &arc, NodeSharedData const &data,
                          PathManipulator &parent);

    /// Read-only access to the geometric arc.
    Geom::EllipticalArc const &arc() const { return _arc; }

    /**
     * Shorten the controlled arc to only the part after the subdivision point,
     * returning a new subdivision node controlling the part before the subdivision point.
     *
     * @param subdivision_time  Curve time in the interval [0, 1].
     */
    std::unique_ptr<Node> subdivideArc(double subdivision_time);

    void setVisible(bool visible);

    void setArcInitialPoint(Geom::Point const &new_point);

    void setArcFinalPoint(Geom::Point const &new_point);

    /// Replace the manipulated arc with a new one.
    void setArcGeometry(Geom::EllipticalArc const &new_arc);

    void commitUndoEvent(CommitEvent event_type) const;

    void updateDisplay();

    /// Feed the manipulated elliptical arc into a path sink.
    void writeSegment(Geom::PathSink &output) const;

private:
    Geom::EllipticalArc _arc;
    NodeSharedData const *_node_shared_data = nullptr;
    CanvasItemPtr<CanvasItemBpath> _contour;
    PathManipulator *_parent = nullptr;
};
} // namespace Inkscape::UI

#endif // INKSCAPE_UI_TOOL_ELLIPTICAL_MANIPULATOR_H

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
