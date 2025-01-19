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

#include "remove-elliptical-arcs.h"

#include <2geom/bezier-curve.h>
#include <2geom/curve.h>
#include <2geom/elliptical-arc.h>
#include <2geom/pathvector.h>
#include <2geom/sbasis-to-bezier.h>
#include <span>
#include <tuple>

namespace Inkscape::UI {

Geom::PathVector remove_elliptical_arcs_if_not_requested(Geom::PathVector pathvector_to_convert,
                                                         std::span<const NodeTypeRequest> requested_node_types)
{
    if (requested_node_types.empty()) {
        return pathvector_to_convert; // By default, we do not kill arcs
    }

    auto const is_ellipse_requested = [&requested_node_types](auto &it) {
        return it == requested_node_types.end() || (it++)->elliptical_arc_requested;
    };

    Geom::PathVector result;
    auto iterator = requested_node_types.begin();

    for (auto const &path : pathvector_to_convert) {
        result.push_back(Geom::Path(path.initialPoint()));
        auto &current_path = result.back();
        current_path.setStitching(true);

        std::ignore = is_ellipse_requested(iterator); // Ignore the first request on a path
        for (auto curve_it = path.begin(); curve_it != path.end_open(); ++curve_it) {
            if (!is_ellipse_requested(iterator) && dynamic_cast<Geom::EllipticalArc const *>(&*curve_it)) {
                // convert this arc to a Bézier path
                auto bezier_path = Geom::cubicbezierpath_from_sbasis(curve_it->toSBasis(), 0.1);
                bezier_path.close(false);
                current_path.append(bezier_path);
            } else {
                current_path.append(*curve_it);
            }
        }
        current_path.close(path.closed());
    }
    return result;
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