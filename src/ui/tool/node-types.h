// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Node types and other small enums.
 * This file exists to reduce the number of includes pulled in by toolbox.cpp.
 */
/* Authors:
 *   Krzysztof Kosiński <tweenk.pl@gmail.com>
 *
 * Copyright (C) 2009 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_UI_TOOL_NODE_TYPES_H
#define SEEN_UI_TOOL_NODE_TYPES_H

namespace Inkscape {
namespace UI {

/** Types of nodes supported in the node tool. */
enum NodeType {
    NODE_CUSP, ///< Cusp node - no handle constraints
    NODE_SMOOTH, ///< Smooth node - handles must be colinear
    NODE_AUTO, ///< Auto node - handles adjusted automatically based on neighboring nodes
    NODE_SYMMETRIC, ///< Symmetric node - handles must be colinear and of equal length
    NODE_LAST_REAL_TYPE, ///< Last real type of node - used for ctrl+click on a node
    NODE_PICK_BEST = 100 ///< Select type based on handle positions
};

/** Types of segments supported in the node tool. */
enum SegmentType {
    SEGMENT_STRAIGHT, ///< Straight linear segment
    SEGMENT_CUBIC_BEZIER ///< Bezier curve with two control points
};

enum class AlignTargetNode {
    LAST_NODE,
    FIRST_NODE,
    MID_NODE,
    MIN_NODE,
    MAX_NODE
};

/// Characters used in the sodipodi:nodetype and LPE-related nodetype XML attributes
enum class XmlNodeType : char
{
    AUTO = 'a',
    BOGUS = 'b',
    CUSP = 'c',
    ELLIPSE_MODIFIER = 'e',
    SMOOTH = 's',
    SYMMETRIC = 'z',
};

inline constexpr XmlNodeType encode_node_type(NodeType type)
{
    switch (type) {
        case NODE_CUSP:
            return XmlNodeType::CUSP;

        case NODE_SMOOTH:
            return XmlNodeType::SMOOTH;

        case NODE_AUTO:
            return XmlNodeType::AUTO;

        case NODE_SYMMETRIC:
            return XmlNodeType::SYMMETRIC;

        default:
            return XmlNodeType::BOGUS;
    }
}

inline constexpr NodeType decode_node_type(XmlNodeType xml_type)
{
    switch (xml_type) {
        case XmlNodeType::AUTO:
            return NODE_AUTO;

        case XmlNodeType::CUSP:
            return NODE_CUSP;

        case XmlNodeType::SMOOTH:
            return NODE_SMOOTH;

        case XmlNodeType::SYMMETRIC:
            return NODE_SYMMETRIC;

        default:
            return NODE_PICK_BEST;
    }
}

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
