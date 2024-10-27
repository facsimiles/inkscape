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

#include "elliptical-arc-end-node.h"

#include <2geom/elliptical-arc.h>

#include "elliptical-arc-handler.h"
#include "inkscape.h"

namespace Inkscape::UI {

EllipticalArcEndNode::EllipticalArcEndNode(Geom::EllipticalArc const &preceding_arc, NodeSharedData const &data)
    : Node{data, preceding_arc.finalPoint()}
    , _manipulator{_desktop ? *_desktop : *SP_ACTIVE_DESKTOP, preceding_arc, data}
{}

std::unique_ptr<CurveHandler> EllipticalArcEndNode::createEventHandlerForPrecedingCurve()
{
    return std::make_unique<EllipticalArcHandler>(_manipulator);
}

void EllipticalArcEndNode::move(Geom::Point const &p)
{
    Node::move(p);
    _manipulator.setArcFinalPoint(p);
}

void EllipticalArcEndNode::notifyPrecedingNodeUpdate(Node &previous_node)
{
    _manipulator.setArcInitialPoint(previous_node.position());
    previous_node.front()->retract();
}

void EllipticalArcEndNode::transform(Geom::Affine const &m)
{
    setPosition(position() * m);
    back()->retract();
    _manipulator.setArcFinalPoint(position());
}

void EllipticalArcEndNode::fixNeighbors()
{
    back()->retract();
}

void EllipticalArcEndNode::showHandles(bool v)
{
    _extra_ui_visible = v;
    _manipulator.setVisible(v);

    if (auto next_node = NodeList::get_iterator(this).next();
        next_node && next_node->handlesAllowedOnPrecedingSegment()) {
        front()->setVisible(v && !front()->isDegenerate());
    }
}

void EllipticalArcEndNode::setType(NodeType, bool)
{
    /// TODO: replace setType with polymorphism
    updateState();
    _manipulator.updateDisplay();
}

std::unique_ptr<Node> EllipticalArcEndNode::subdivideArc(double curve_time)
{
    auto result = _manipulator.subdivideArc(curve_time);
    result->setType(NODE_CUSP, false);
    return result;
}

void EllipticalArcEndNode::writeSegment(Geom::PathSink &output, const Node &) const
{
    _manipulator.writeSegment(output);
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
