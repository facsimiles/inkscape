// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Functions to encode paths and shape information into pdf
 *
 * Authors:
 *   Martin Owens <doctormo@geek-2.com>
 *
 * Copyright (C) 2024 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "pdf-builder.h"

#include <2geom/sbasis-to-bezier.h>

using Geom::X;
using Geom::Y;

namespace Inkscape::Extension::Internal::PdfBuilder {

/**
 * Render a pathvector to a CapyPDF file.
 */
bool DrawContext::render_pathvector(Geom::PathVector const &pathv)
{
    if (pathv.empty()) {
        return false;
    }

    bool closed = true;

    for (auto const &path : pathv) {
        if (path.empty()) {
            continue;
        }

        _ctx.cmd_m(path.initialPoint().x(), path.initialPoint().y());

        render_path(path);

        if (path.closed()) {
            _ctx.cmd_h();
        } else {
            closed = false;
        }
    }

    return closed;
}

void DrawContext::render_path(Geom::Path const &path)
{
    for (Geom::Curve const &iter : path) {
        if (auto const bezier = dynamic_cast<Geom::BezierCurve const *>(&iter)) {
            auto const order = bezier->order();
            switch (order) {
                case 1: // Line
                    _ctx.cmd_l(bezier->finalPoint()[0], bezier->finalPoint()[1]);
                    break;
                case 2: // Quadratic bezier
                {
                    // degree-elevate to cubic Bezier, since PDF doesn't do quadratic Beziers
                    Geom::Point b1 = bezier->controlPoint(0) + (2./3) * (bezier->controlPoint(1) - bezier->controlPoint(0));
                    Geom::Point b2 = b1 + (1./3) * (bezier->controlPoint(2) - bezier->controlPoint(0));
                    _ctx.cmd_c(b1[X], b1[Y], b2[X], b2[Y], bezier->controlPoint(2)[X], bezier->controlPoint(2)[Y]);
                    break;
                }
                case 3: // Cubic bezier
                    _ctx.cmd_c(
                        bezier->controlPoint(1)[X], bezier->controlPoint(1)[Y],
                        bezier->controlPoint(2)[X], bezier->controlPoint(2)[Y],
                        bezier->controlPoint(3)[X], bezier->controlPoint(3)[Y]);
                    break;
                default:
                    // handles sbasis as well as all other curve types, note this is very slow
                    // recurse to convert the new path resulting from the sbasis to svgd
                    render_path(Geom::cubicbezierpath_from_sbasis(bezier->toSBasis(), 0.1));
                    break;
            }
        } else if (auto const arc = dynamic_cast<Geom::EllipticalArc const *>(&iter)) {
            render_path(Geom::cubicbezierpath_from_sbasis(arc->toSBasis(), 0.1));
        }
    }
}

} // namespace Inkscape::Extension::Internal::PdfBuilder
