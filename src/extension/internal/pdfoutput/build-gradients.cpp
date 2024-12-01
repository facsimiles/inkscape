// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Gradient functions to generate pdf shading funtions.
 *
 * Authors:
 *   Martin Owens <doctormo@geek-2.com>
 *
 * Copyright (C) 2024 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "pdf-builder.h"

#include <array>

#include "object/sp-linear-gradient.h"
#include "object/sp-radial-gradient.h"
#include "object/sp-mesh-gradient.h"
#include "object/sp-gradient.h"

using Geom::X;
using Geom::Y;

namespace Inkscape::Extension::Internal::PdfBuilder {

/**
 * Return true if the SVG gradient is repeated and reflected, false if it's repeated but not reflected.
 */
static std::optional<bool> repeat_is_reflection(SPGradientSpread spread)
{
    switch(spread) {
        case SP_GRADIENT_SPREAD_REFLECT:
            return true;
        case SP_GRADIENT_SPREAD_REPEAT:
            return false;
        default: // Everything else is none
            break;
    }
    return {};
}

/**
 * Returns true if the gradient has transparency.
 */
bool gradient_has_transparency(SPPaintServer const *paint)
{
    if (!paint || !paint->isValid())
        return false;

    if (auto linear = cast<SPLinearGradient>(paint)) {
        for (auto &stop : linear->getGradientVector().stops) {
            if (stop.color->getOpacity() < 1.0) {
                return true;
            }
        }
    } else if (auto radial = cast<SPRadialGradient>(paint)) {
        for (auto &stop : radial->getGradientVector().stops) {
            if (stop.color->getOpacity() < 1.0) {
                return true;
            }
        }
    } else if (auto mesh = cast<SPMeshGradient>(paint)) {
        SPMeshNodeArray array;
        array.read(const_cast<SPMeshGradient *>(mesh));
        for (auto &a : array.nodes) {
            for (auto &b : a) {
                if (b->color && b->color->getOpacity() < 1.0) {
                    return true;
                }
            }
        }
    }

    return false;
}

/**
 * Construct a PDF pattern object from the given paintserver. Gradients or patterns.
 *
 * @arg paint - The paint server vector.
 * @arg bbox - The bounding box for this pattern
 * @arg opacity - The total paint opacity, used for softmasking. Only used for soft-masking
 */
std::optional<CapyPDF_PatternId> Document::get_pattern(SPPaintServer const *paint, Geom::Rect const &bbox, std::optional<double> opacity)
{
    if (!paint || !paint->isValid())
        return {};

    auto to_userspace = Geom::Affine(bbox.width(), 0, 0, bbox.height(), bbox.left(), bbox.top());
    auto color_space = get_default_colorspace();

    // This is a soft mask if opacity is set
    if (opacity) {
        color_space = CAPY_DEVICE_CS_GRAY;
    }

    if (auto linear = cast<SPLinearGradient>(paint)) {
        auto line = linear->getLine();
        auto cm = linear->gradientTransform;

        if (linear->getUnits() == SP_GRADIENT_UNITS_OBJECTBOUNDINGBOX) { 
            cm *= to_userspace;
        }

        if (auto func = get_gradient_function(linear->getGradientVector(), opacity, &color_space)) {
            auto d1 = 0.0;
            auto d2 = 1.0;

            if (auto is_reflection = repeat_is_reflection(linear->fetchSpread())) {
                // Bounding box is already in cm coords, so line must be transformed to compare them.
                auto line_cm = line * cm;
                auto o1 = line_cm.origin();
                auto o2 = (line_cm.origin() + line_cm.vector());

                // Select the corners to calculate from
                int d1_corner = ((o1[X] < o2[X]) != (o1[Y] < o2[Y])) + ((o1[Y] >= o2[Y]) * 2);
                int d2_corner = (d1_corner + 2) % 4;

                // This maths was by Krlr17, with many thanks.
                d1 = line_cm.timeAtProjection(bbox.corner(d1_corner));
                d2 = line_cm.timeAtProjection(bbox.corner(d2_corner));

                func = get_repeat_function(*func, *is_reflection, std::floor(d1), std::ceil(d2));
            }

            // In PDF the shading space must be the total function space include repeats
            auto p1 = line.origin() + (line.vector() * d1);
            auto p2 = line.origin() + (line.vector() * d2);

            // x1, y1, x2, y2, interpolation function, extend past x1/y1, exend past x2/y2
            auto shading = capypdf::Type2Shading(color_space, p1[X], p1[Y], p2[X], p2[Y], *func);
            shading.set_extend(true, true);
            shading.set_domain(d1, d2);
            auto sid = _gen.add_shading(shading);

            auto pattern = capypdf::ShadingPattern(sid);
            pattern.set_matrix(cm[0], cm[1], cm[2], cm[3], cm[4], cm[5]);
            return _gen.add_shading_pattern(pattern);
        }
    } else if (auto radial = cast<SPRadialGradient>(paint)) {

        auto cm = radial->gradientTransform;
        Geom::Point center (radial->cx.computed, radial->cy.computed);
        Geom::Point focal (radial->fx.computed, radial->fy.computed);

        double r  = radial->r.computed;
        double fr = radial->fr.computed;

        if (radial->getUnits() == SP_GRADIENT_UNITS_OBJECTBOUNDINGBOX) {
            cm *= to_userspace;
        }

        if (auto func = get_gradient_function(radial->getGradientVector(), opacity, &color_space)) {
            double d1 = 0.0;
            double d2 = 1.0;

            if (auto is_reflection = repeat_is_reflection(radial->fetchSpread())) {

                // Process X and Y radii to find the smallest one after transformation
                for (auto r_vector : {Geom::Point(r, 0.0), Geom::Point(0.0, r)}) {
                    // Bounding box is already in cm coords, so radial must be transformed to compare them.
                    auto line = Geom::Line::from_origin_and_vector(center, r_vector) * cm;
                    auto r_cm = Geom::distance(line.origin(), line.origin() + line.vector());

                    // Given the smallest radius and the largest distance to a corner, get repeats
                    for(auto i : {0, 1, 2, 3}) { // Each bbox corner
                        // Number of steps to reach that corner from the center
                        auto steps = Geom::distance((bbox * cm).corner(i), line.origin()) / r_cm + 1;
                        if (steps > d2) {
                            d2 = steps;
                        }
                    }
                }
                func = get_repeat_function(*func, *is_reflection, std::floor(d1), std::ceil(d2));
            }

            // Coord radius is scaled to include repeating function
            auto coords = std::to_array({focal[X], focal[Y], fr * d2, center[X], center[Y], r * d2});

            auto shading = capypdf::Type3Shading(color_space, coords.data(), coords.size(), *func);
            shading.set_extend(true, true);
            shading.set_domain(d1, d2);
            auto sid = _gen.add_shading(shading);
            auto pattern = capypdf::ShadingPattern(sid);
            pattern.set_matrix(cm[0], cm[1], cm[2], cm[3], cm[4], cm[5]);
            return _gen.add_shading_pattern(pattern);
        }
    } else if (auto mesh = cast<SPMeshGradient>(paint)) {

        auto cm = mesh->gradientTransform;

        SPMeshNodeArray array;
        // TODO: Find out why array read can't be const.
        array.read(const_cast<SPMeshGradient *>(mesh));

        // The first node is given as the color space of he whole gradient
        auto space = array.nodes[0][0]->color->getSpace();
        if (!opacity) {
            color_space = get_colorspace(space);
        }

        auto box = bbox * cm.inverse();
        auto shading = capypdf::Type6Shading(color_space, box.left(), box.bottom(), box.right(), box.top());

        for (unsigned i = 0; i < array.patch_rows(); i++) {
            for (unsigned j = 0; j < array.patch_columns(); j++) {
                auto patch = SPMeshPatchI(&array.nodes, i, j);
                std::vector<double> coords;
                std::vector<capypdf::Color> colors;

                for (int k = 0; k < 4; k++) {
                    if (patch.tensorIsSet(k)) {
                        g_warning("Can't set tensor for Type7Shading, not supported yet.");
                    }

                    // We only have 24 slots, not 32, we think the last point is a duplicate or zero
                    for (unsigned l : {0, 1, 2}) {
                        auto p = patch.getPoint(k, l);
                        coords.emplace_back(p[X]);
                        coords.emplace_back(p[Y]);
                    }
                    colors.push_back(get_color(*patch.getColor(k)->converted(space), opacity));
                }

                shading.add_patch(coords.data(), coords.size(), colors.data(), colors.size());

            }
        }

        auto sid = _gen.add_shading(shading);
        auto pattern = capypdf::ShadingPattern(sid);
        pattern.set_matrix(cm[0], cm[1], cm[2], cm[3], cm[4], cm[5]);
        return _gen.add_shading_pattern(pattern);
    }

    return {};
}

/**
 * Generate a non-continous gradient from the gradient vector and add to the document.
 *
 * @returns The FunctionId for the new gradient and sets the pdf_space Color Space if needed.
 */
std::optional<CapyPDF_FunctionId> Document::get_gradient_function(SPGradientVector const &vector, std::optional<double> opacity, CapyPDF_DeviceColorspace *pdf_space)
{
    static auto domain = std::to_array({0.0, 1.0});
    unsigned int stops = vector.stops.size();
    auto color_space = vector.stops[0].color->getSpace();

    // Update the caller on what the color space should be for this gradient
    if (!opacity) {
        *pdf_space = get_colorspace(color_space);
    }

    // Type3 Function, a collection of Type2 functions between each color stop pair
    std::vector<CapyPDF_FunctionId> functs;
    std::vector<double> bounds;
    std::vector<double> encode;

    for (unsigned int i = 0; i < stops - 1 || i == 0; i++) {

        auto c1 = get_color(*vector.stops[i].color->converted(color_space), opacity);
        // A single stop gradient is a swatch, this makes a gradient but it might be better as a Spot Color
        auto c2 = get_color(*vector.stops[i+(stops == 1 ? 0 : 1)].color->converted(color_space), opacity);

        auto func = capypdf::Type2Function(domain.data(), domain.size(), c1, c2);
        functs.push_back(_gen.add_function(func));
        encode.emplace_back(0.0);
        encode.emplace_back(1.0);

        if (i != 0) {
            bounds.emplace_back(vector.stops[i].offset);
        }
    }

    // One function means a single stop pair, one Type2 Function is enough
    if (functs.size() == 1) {
        return functs[0];
    }

    // A type3 Functon allows for more than 2 color stops in one gradient.
    auto func = capypdf::Type3Function(
            domain.data(), domain.size(),
            functs.data(), functs.size(),
            bounds.data(), bounds.size(),
            encode.data(), encode.size());
    return _gen.add_function(func);
}

/**
 * Generate a type3 gradient function which repeats the given gradient for the given ranges.
 */
std::optional<CapyPDF_FunctionId> Document::get_repeat_function(CapyPDF_FunctionId gradient, bool reflected, int from, int to)
{
    auto domain = std::to_array<double>({from, to});
    std::vector<CapyPDF_FunctionId> functs;
    std::vector<double> bounds;
    std::vector<double> encode;

    for (auto i = from + 1; i <= to; i++) {
        functs.push_back(gradient);
        if (i < to) {
            bounds.push_back(i);
        }
        if (reflected && i % 2 == 0) {
            encode.push_back(1.0);
            encode.push_back(0.0);
        } else {
            encode.push_back(0.0);
            encode.push_back(1.0);
        }
    }
    if (functs.empty()) {
        return gradient;
    }

    auto func = capypdf::Type3Function(
            domain.data(), domain.size(),
            functs.data(), functs.size(),
            bounds.data(), bounds.size(),
            encode.data(), encode.size());
    return _gen.add_function(func);
}

} // namespace Inkscape::Extension::Internal::PdfBuilder
