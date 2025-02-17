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

#ifndef INKSCAPE_UI_TOOL_NODE_FACTORY_H
#define INKSCAPE_UI_TOOL_NODE_FACTORY_H

#include <memory>
#include <span>
#include <vector>

#include "node-types.h"

class SPDesktop;

namespace Geom {
class Curve;
class Path;
} // namespace Geom

namespace Inkscape {

class CanvasItemGroup;

namespace UI {

class Node;
class PathManipulator;
class SubpathList;
class ControlPointSelection;

struct NodeSharedData
{
    SPDesktop *desktop;
    ControlPointSelection *selection;
    CanvasItemGroup *node_group;
    CanvasItemGroup *handle_group;
    CanvasItemGroup *handle_line_group;
};

struct NodeTypeRequest
{
    XmlNodeType requested_type = XmlNodeType::BOGUS;
    bool elliptical_arc_requested = false;
};

/// Convert the content of the node type attribute in XML to a list of node request objects.
std::vector<NodeTypeRequest> read_node_type_requests(char const *xml_node_type_string);

/// Set the node types in the passed list of subpaths according to the passed requests.
void set_node_types(SubpathList &subpath_list, std::span<const NodeTypeRequest> requests);

class NodeFactory
{
public:
    NodeFactory(std::span<const NodeTypeRequest> request_sequence, PathManipulator *manipulator);

    NodeFactory(NodeFactory const &) = delete;
    NodeFactory &operator=(NodeFactory const &) = delete;

    /**
     * Create the initial node at the beginning of a path
     */
    std::unique_ptr<Node> createInitialNode(Geom::Path const &path);

    /**
     * Create a new node at the endpoint of the passed curve,
     * consuming one element of the node type request sequence.
     */
    std::unique_ptr<Node> createNextNode(Geom::Curve const &preceding_curve);

    /**
     * Create a node controlling an elliptical arc. The node will be placed at the endpoint
     * of the passed curve, and the elliptical arc will attempt to approximate the curve,
     * or will be a semicircle if the curve is a straight line.
     */
    std::unique_ptr<Node> createArcEndpointNode(Geom::Curve const &curve);

private:
    NodeTypeRequest _getNextRequest();

    PathManipulator *_manipulator = nullptr;
    NodeSharedData _shared_data;
    std::span<const NodeTypeRequest> _requests;
    unsigned _pos = 0;
    bool _always_create_elliptical_arcs = false;
};
} // namespace UI
} // namespace Inkscape

#endif

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