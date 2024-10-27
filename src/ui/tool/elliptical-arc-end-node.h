// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Editable node at the end of an elliptical arc.
 */
/* Authors:
 *   Rafał M. Siejakowski <rs@rs-math.net>
 *
 * Copyright (C) 2024 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_TOOL_ELLIPTICAL_ARC_END_NODE_H
#define INKSCAPE_UI_TOOL_ELLIPTICAL_ARC_END_NODE_H

#include "elliptical-manipulator.h"
#include "ui/tool/node.h"

namespace Geom {
class Affine;
class EllipticalArc;
class Point;
} // namespace Geom

namespace Inkscape::UI {
class NodeSharedData;
class CurveHandler;

class EllipticalArcEndNode : public Node {
public:
    EllipticalArcEndNode(Geom::EllipticalArc const &preceding_arc, NodeSharedData const &data);

    ~EllipticalArcEndNode() override = default;

    void move(Geom::Point const &p) final;

    void transform(Geom::Affine const &m) final;

    void fixNeighbors() final;

    void showHandles(bool v) final;

    bool isPrecedingSegmentStraight() const final { return false; }

    bool handlesAllowedOnPrecedingSegment() const final { return false; }

    void notifyPrecedingNodeUpdate(Node &previous_node) final;

    void setType(NodeType, bool) final;

    /// Subdivide the arc preceding this node and return a new node at the prescribed curve time parameter.
    std::unique_ptr<Node> subdivideArc(double curve_time);

    void writeSegment(Geom::PathSink &output, Node const &) const final;

    std::unique_ptr<CurveHandler> createEventHandlerForPrecedingCurve() final;

    bool areHandlesVisible() const final { return _extra_ui_visible; }

private:
    EllipticalManipulator _manipulator;
    bool _extra_ui_visible = false;
};
}

#endif // INKSCAPE_UI_TOOL_ELLIPTICAL_ARC_END_NODE_H

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
