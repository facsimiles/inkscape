// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Factory for creating Node objects for the Node Tool
 */
/* Authors:
 *   Rafał M. Siejakowski <rs@rs-math.net>
 *
 * Copyright (C) 2025 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "node-factory.h"

#include <2geom/bezier-curve.h>
#include <2geom/curve.h>
#include <2geom/elliptical-arc.h>
#include <2geom/exception.h>
#include <span>
#include <string_view>
#include <tuple>
#include <vector>

#include "elliptical-arc-end-node.h"
#include "node.h"
#include "ui/tool/path-manipulator.h"

namespace Inkscape::UI {

namespace {

Geom::EllipticalArc make_semicircle(Geom::Point const &from, Geom::Point const &to)
{
    auto const r = 0.5 * Geom::distance(from, to);
    return {from, r, r, 0.0, true, true, to};
}

std::unique_ptr<Geom::EllipticalArc> fit_arc_to_cubic_bezier(Geom::CubicBezier const &bezier)
{
    assert(!bezier.isLineSegment());

    auto const initial_point = bezier.initialPoint();
    auto const mid_point = bezier.pointAt(0.5);
    auto const final_point = bezier.finalPoint();

    Geom::Ellipse ellipse;
    try {
        const std::vector points{initial_point, bezier.pointAt(0.25), mid_point, bezier.pointAt(0.75), final_point};
        ellipse.fit(points);
    } catch (Geom::RangeError const &) {
        return {};
    }

    return std::unique_ptr<Geom::EllipticalArc>{ellipse.arc(initial_point, mid_point, final_point)};
}
} // namespace

std::vector<NodeTypeRequest> read_node_type_requests(char const *xml_node_type_string)
{
    std::string_view const node_type_str = xml_node_type_string ? xml_node_type_string : "";

    std::vector<NodeTypeRequest> result;
    result.reserve(node_type_str.size());

    auto const parse_symbol = [&node_type_str](auto &it) -> NodeTypeRequest {
        if (it == node_type_str.end()) {
            return {};
        }

        NodeTypeRequest result;

        while (it != node_type_str.end() && *it == static_cast<char>(XmlNodeType::ELLIPSE_MODIFIER)) {
            result.elliptical_arc_requested = true;
            ++it;
        }
        if (it == node_type_str.end()) {
            return result;
        }

        result.requested_type = static_cast<XmlNodeType>(*it++);
        return result;
    };

    auto iterator = node_type_str.begin();
    while (iterator != node_type_str.end()) {
        result.push_back(parse_symbol(iterator));
    }
    return result;
}

void set_node_types(SubpathList &subpath_list, std::span<const NodeTypeRequest> requests)
{
    auto const get_next = [&requests](auto &it) { return it == requests.end() ? NodeTypeRequest{} : *it++; };

    auto iterator = requests.begin();
    for (auto &subpath : subpath_list) {
        for (auto &node : *subpath) {
            const auto [node_type, ellipse_modifier] = get_next(iterator);
            node.setType(decode_node_type(node_type), false);
        }
        if (subpath->closed()) {
            // STUPIDITY ALERT: it seems we need to use the duplicate type symbol instead of
            // the first one to remain backward compatible.
            const auto [node_type, ellipse_modifier] = get_next(iterator);
            if (node_type != XmlNodeType::BOGUS) {
                subpath->begin()->setType(decode_node_type(node_type), false);
            }
        }
    }
}

NodeFactory::NodeFactory(std::span<const NodeTypeRequest> request_sequence, PathManipulator *manipulator)
    : _manipulator{manipulator}
    , _requests{request_sequence}
    , _always_create_elliptical_arcs{request_sequence.empty()}
{
    assert(_manipulator);
    _shared_data = _manipulator->getNodeSharedData();
}

std::unique_ptr<Node> NodeFactory::createInitialNode(Geom::Path const &path)
{
    std::ignore = _getNextRequest();

    // Initial node is always of base type; we can change it upon reaching the end of a closed path
    return std::make_unique<Node>(_shared_data, path.initialPoint());
}

std::unique_ptr<Node> NodeFactory::createNextNode(Geom::Curve const &preceding_curve)
{
    auto const type_request = _getNextRequest();
    if (type_request.elliptical_arc_requested || _always_create_elliptical_arcs) {
        if (auto const *arc = dynamic_cast<Geom::EllipticalArc const *>(&preceding_curve)) {
            return std::make_unique<EllipticalArcEndNode>(*arc, _shared_data, *_manipulator);
        }
    }
    return std::make_unique<Node>(_shared_data, preceding_curve.finalPoint());
}

std::unique_ptr<Node> NodeFactory::createArcEndpointNode(Geom::Curve const &curve)
{
    Geom::EllipticalArc arc = make_semicircle(curve.initialPoint(), curve.finalPoint());
    if (!curve.isLineSegment()) {
        if (const auto *cubic_bezier = dynamic_cast<Geom::CubicBezier const *>(&curve)) {
            if (const auto fitted = fit_arc_to_cubic_bezier(*cubic_bezier)) {
                arc = *fitted;
            }
        } else if (const auto *already_arc = dynamic_cast<Geom::EllipticalArc const *>(&curve)) {
            arc = *already_arc;
        }
    }
    return std::make_unique<EllipticalArcEndNode>(arc, _shared_data, *_manipulator);
}

NodeTypeRequest NodeFactory::_getNextRequest()
{
    if (_pos < _requests.size()) {
        return _requests[_pos++];
    }
    return {};
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