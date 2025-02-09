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
#include <array>
#include <cmath>

#include "elliptical-arc-handler.h"
#include "inkscape.h"
#include "node-factory.h"
#include "object/sp-item.h"
#include "path-manipulator.h"
#include "ui/tool/node-types.h"
#include "util/cast.h"

namespace Inkscape::UI {

namespace {

using CubicBezierIntermediateControlPoints = std::array<Geom::Point, 2>;

/// Given an elliptical arc, compute the two intermediate control points of a cubic Bézier approximating the arc.
CubicBezierIntermediateControlPoints
compute_cubic_bezier_control_points_for_arc_approximation(Geom::EllipticalArc const &arc)
{
    auto const quarter_angle = arc.sweepAngle() / 4.0;
    if (Geom::are_near(quarter_angle, M_PI_2) || Geom::are_near(quarter_angle, -M_PI_2)) {
        return {arc.initialPoint(), arc.finalPoint()};
    }
    auto const sweep_angle_factor = 4.0 * tan(quarter_angle) / 3.0;

    auto const unit_arc_start = Geom::Point::polar(arc.initialAngle());
    auto const unit_arc_end = Geom::Point::polar(arc.finalAngle());
    auto const tr = arc.unitCircleTransform();

    return {(unit_arc_start + sweep_angle_factor * unit_arc_start.cw()) * tr,
            (unit_arc_end - sweep_angle_factor * unit_arc_end.cw()) * tr};
}

void set_up_nodes_for_arc_replacement(Node *end_node_of_replacement_curve, SegmentType requested_segment_type,
                                      NodeType requested_node_type, Geom::Point const &front_handle_position,
                                      Geom::EllipticalArc const &arc)
{
    end_node_of_replacement_curve->setType(requested_node_type, false);
    end_node_of_replacement_curve->front()->move(front_handle_position);

    if (requested_segment_type == SEGMENT_CUBIC_BEZIER) {
        // Set up handles for a Bézier segment
        auto *start_node_of_replacement_curve =
            end_node_of_replacement_curve->nodeToward(end_node_of_replacement_curve->back());

        auto const handle_positions = compute_cubic_bezier_control_points_for_arc_approximation(arc);
        end_node_of_replacement_curve->back()->move(handle_positions.back());
        start_node_of_replacement_curve->front()->move(handle_positions.front());
    }
}
} // namespace

EllipticalArcEndNode::EllipticalArcEndNode(Geom::EllipticalArc const &preceding_arc, NodeSharedData const &data,
                                           SPObject const *path, PathManipulator &parent)
    : Node{data, preceding_arc.finalPoint()}
    , _manipulator{_desktop ? *_desktop : *SP_ACTIVE_DESKTOP, preceding_arc, data, cast<SPItem>(path), parent}
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

void EllipticalArcEndNode::changePrecedingSegmentType(SegmentType new_type, Node &preceding_node)
{
    if (new_type == SEGMENT_ELLIPTICAL) {
        return; // Nothing to do
    }

    // Before committing suicide, copy required data onto the stack.
    auto const node_type = type();
    auto const arc = _manipulator.arc();
    auto const front_handle_old_position = front()->position();

    // Make a new node to replace self.
    auto *replacement_node = _pm().createNodeFactory().createNextNode(arc.chord()).release();
    std::move(*this)._replace(replacement_node);

    set_up_nodes_for_arc_replacement(replacement_node, new_type, node_type, front_handle_old_position, arc);
}

void EllipticalArcEndNode::setType(NodeType type, bool)
{
    if (type == NODE_PICK_BEST) {
        type = NODE_CUSP;
    }
    Node::setType(type, false);
    /// TODO: update handles manually

    updateState();
    _manipulator.updateDisplay();
}

void EllipticalArcEndNode::writeType(std::ostream &output_stream) const
{
    output_stream << static_cast<char>(XmlNodeType::ELLIPSE_MODIFIER);
    Node::writeType(output_stream);
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
