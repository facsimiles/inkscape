// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Convert elliptical arcs to Béziers if elliptical arcs are unwelcome
 */
/* Authors:
 *   Rafał M. Siejakowski <rs@rs-math.net>
 *
 * Copyright (C) 2025 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_TOOL_CURVE_CONVERTER_H
#define INKSCAPE_UI_TOOL_CURVE_CONVERTER_H

#include <2geom/pathvector.h>
#include <span>

#include "node-factory.h"

namespace Inkscape::UI {

/**
 * Convert elliptical arcs in a PathVector to Bézier curves if the corresponding node type request for the
 * Node Tool does not call for the creation of elliptical arc controls.
 *
 * This preserves the historical behaviour of the Node Tool on old SVG documents (which do not have the character
 * 'e' in the sodipodi:nodetypes attribute) and after an ellipse is converted to a path manually.
 */
Geom::PathVector remove_elliptical_arcs_if_not_requested(Geom::PathVector pathvector_to_convert,
                                                         std::span<NodeTypeRequest const> requested_node_types);

} // namespace Inkscape::UI

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