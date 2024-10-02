// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Provide a capypdf interface that understands 2geom, styles, etc.
 *
 * Authors:
 *   Martin Owens <doctormo@geek-2.com>
 *
 * Copyright (C) 2024 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "pdf-builder.h"

#include "style.h"
#include "colors/color.h"
#include "colors/spaces/cmyk.h"
#include "colors/spaces/cms.h"
#include "colors/cms/profile.h"

#include "object/sp-paint-server.h"

namespace Inkscape::Extension::Internal::PdfBuilder {

/**
 * Get the blend mode for capyPDF output.
 */
static std::optional<CapyPDF_Blend_Mode> get_blendmode(SPBlendMode mode)
{
    switch (mode)
    {
        case SP_CSS_BLEND_MULTIPLY:
            return CAPY_BM_MULTIPLY;
        case SP_CSS_BLEND_SCREEN:
            return CAPY_BM_SCREEN;
        case SP_CSS_BLEND_DARKEN:
            return CAPY_BM_DARKEN;
        case SP_CSS_BLEND_LIGHTEN:
            return CAPY_BM_LIGHTEN;
        case SP_CSS_BLEND_OVERLAY:
            return CAPY_BM_OVERLAY;
        case SP_CSS_BLEND_COLORDODGE:
            return CAPY_BM_COLORDODGE;
        case SP_CSS_BLEND_COLORBURN:
            return CAPY_BM_COLORBURN;
        case SP_CSS_BLEND_HARDLIGHT:
            return CAPY_BM_HARDLIGHT;
        case SP_CSS_BLEND_SOFTLIGHT:
            return CAPY_BM_SOFTLIGHT;
        case SP_CSS_BLEND_DIFFERENCE:
            return CAPY_BM_DIFFERENCE;
        case SP_CSS_BLEND_EXCLUSION:
            return CAPY_BM_EXCLUSION;
        case SP_CSS_BLEND_HUE:
            return CAPY_BM_HUE;
        case SP_CSS_BLEND_SATURATION:
            return CAPY_BM_SATURATION;
        case SP_CSS_BLEND_COLOR:
            return CAPY_BM_COLOR;
        case SP_CSS_BLEND_LUMINOSITY:
            return CAPY_BM_LUMINOSITY;
        default:
            break;
    }
    return {};
}

static CapyPDF_Image_Interpolation get_interpolation(SPImageRendering rendering)
{
    switch (rendering) {
        case SP_CSS_IMAGE_RENDERING_OPTIMIZEQUALITY:
            return CAPY_INTERPOLATION_SMOOTH;
            break;
        case SP_CSS_IMAGE_RENDERING_OPTIMIZESPEED:
        case SP_CSS_IMAGE_RENDERING_PIXELATED:
        case SP_CSS_IMAGE_RENDERING_CRISPEDGES:
            return CAPY_INTERPOLATION_PIXELATED;
            break;
        case SP_CSS_IMAGE_RENDERING_AUTO:
        default:
            break;
    }
    return CAPY_INTERPOLATION_AUTO;
}


/**
 * Returns true if the gradient has transparency.
 */
bool style_has_gradient_transparency(SPStyle *style)
{
    if (style->fill.set && style->fill.href) {
        if (gradient_has_transparency(style->fill.href->getObject())) {
            return true;
        }
    }
    if (style->stroke.set && style->stroke.href) {
        if (gradient_has_transparency(style->stroke.href->getObject())) {
            return true;
        }
    }
    return false;
}


/**
 * Set the style for any graphic from the SVG style
 *
 * @param style - The SPStyle for this SPObject
 *
 * @returns A GraphicsStateId for the object added to the document, or none if none is needed.
 */
std::optional<CapyPDF_GraphicsStateId> Document::get_child_graphics_state(SPStyle *style, std::optional<CapyPDF_TransparencyGroupId> soft_mask)
{
    if (!style) {
        return {};
    }

    auto gstate = capypdf::GraphicsState();
    bool gs_used = false;

    if (soft_mask) {
        gstate.set_SMask(_gen.add_soft_mask(*soft_mask));
        gs_used = true;
    }
    if (auto blend_mode = get_blendmode(style->mix_blend_mode.value)) {
        gstate.set_BM(*blend_mode);
        gs_used = true;
    }
    if (style->opacity < 1.0) {
        gstate.set_ca(style->opacity);
        gs_used = true;
    }
    if (gs_used) {
        return _gen.add_graphics_state(gstate);
    }

    return {};
}

/**
 * Like get_graphics_style but for drawing shapes (paths)
 *
 * @param style - The style from the SPObject
 * @param soft_mask - The pre-rendered soft mask, i.e. the gradient transparencies.
 *
 * @returns the GraphicsStateId for the object added to the document, or none if not needed.
 */
std::optional<CapyPDF_GraphicsStateId> Document::get_shape_graphics_state(SPStyle *style, std::optional<CapyPDF_TransparencyGroupId> soft_mask)
{
    // PDF allows a lot more to exist in the graphics state, but capypdf does not allow them
    // to be added into the gs and instead they get added directly to the draw context obj.
    auto gstate = capypdf::GraphicsState();
    bool gs_used = false;

    if (soft_mask) {
        gstate.set_SMask(_gen.add_soft_mask(*soft_mask));
        gs_used = true;
    } else { // The draw opacities can not be set at the same time as a soft mask
        if (style->fill_opacity < 1.0) {
            gstate.set_ca(style->fill_opacity);
            gs_used = true;
        }
        if (style->stroke_opacity < 1.0) {
            gstate.set_CA(style->stroke_opacity);
            gs_used = true;
        }
    }
    if (gs_used) {
        return _gen.add_graphics_state(gstate);
    }
    return {};
}

/**
 * Generate a solid color, gradient or pattern based on the SPIPaint
 */
std::optional<capypdf::Color> Document::get_paint(SPIPaint const &paint, Geom::Rect const &bbox, std::optional<double> opacity)
{
    if (paint.isNone())
        return {};

    if (paint.isColor()) {
        return get_color(paint.getColor(), opacity);
    }

    capypdf::Color out;
    if (paint.isPaintserver()) {
        if (auto pattern_id = get_pattern(paint.href ? paint.href->getObject() : nullptr, bbox, opacity)) {
            out.set_pattern(*pattern_id);
        } else {
            g_warning("Couldn't generate pattern for fill '%s'", paint.get_value().c_str());
        }
    } else {
        g_warning("Fill style not supported: '%s'", paint.get_value().c_str());
        out.set_rgb(0, 0, 0); // Black default on error
    }
    return out;
}

capypdf::Color Document::get_color(Colors::Color const &color, std::optional<double> opacity)
{
    auto space = color.getSpace();

    capypdf::Color out;
    if (opacity) {
        out.set_gray(*opacity * color.getOpacity());
    } else if (auto cmyk = std::dynamic_pointer_cast<Colors::Space::DeviceCMYK>(space)) {
        out.set_cmyk(color[0], color[1], color[2], color[3]);
    } else if (auto cms = std::dynamic_pointer_cast<Colors::Space::CMS>(space)) {
        if (auto icc_id = get_icc_profile(cms)) {
            auto vals = color.getValues();
            out.set_icc(*icc_id, vals.data(), vals.size());
        } else {
            g_warning("Couldn't set icc color, icc profile didn't load.");
        }
    } else if (auto rgb = color.converted(Colors::Space::Type::RGB)) {
        out.set_rgb(rgb->get(0), rgb->get(1), rgb->get(2));
    } else {
        g_warning("Problem outputting color '%s' to PDF.", color.toString().c_str());
        out.set_rgb(0, 0, 0); // Black default on error
    }
    return out;
}

std::optional<CapyPDF_IccColorSpaceId> Document::get_icc_profile(std::shared_ptr<Colors::Space::CMS> const &profile)
{
    auto key = profile->getName();
    if (auto it = _icc_cache.find(key); it != _icc_cache.end()) {
        return it->second;
    }

    if (auto cms_profile = profile->getProfile()) {
        auto channels = profile->getComponentCount();
        auto vec = cms_profile->dumpData();
        auto id = _gen.add_icc_profile(std::string_view(reinterpret_cast<const char*>(vec.data()), vec.size()), channels);
        _icc_cache[key] = id; 
        return id; 
    }   
    return {}; 
}

CapyPDF_DeviceColorspace Document::get_default_colorspace()
{
    // TODO: Make this return the correct color space (icc, etc) for the document
    return CAPY_DEVICE_CS_RGB;
}

CapyPDF_DeviceColorspace Document::get_colorspace(std::shared_ptr<Colors::Space::AnySpace> const &space)
{
    if (std::dynamic_pointer_cast<Colors::Space::DeviceCMYK>(space)) {
        return CAPY_DEVICE_CS_CMYK;
    } else if (std::dynamic_pointer_cast<Colors::Space::RGB>(space)) {
        return CAPY_DEVICE_CS_RGB;
    } else if (auto cms = std::dynamic_pointer_cast<Colors::Space::CMS>(space)) {
        // TODO: Support icc profiles here, which are missing from capypdf atm
        g_warning("ICC profile color space expressed as device color space!");
        switch (cms->getType()) {
            case Colors::Space::Type::RGB:
                return CAPY_DEVICE_CS_RGB;
            case Colors::Space::Type::CMYK:
                return CAPY_DEVICE_CS_CMYK;
            default:
                break;
        }
        // Return IccColorSpaceId here, somehow.
    }
    return CAPY_DEVICE_CS_RGB;
}

/**
 * Set the style for drawing shapes from the SVG style, this is all the styles
 * that relate to how vector paths are drawn with stroke, fill and other shape
 * properties. But NOT item styles such as opacity, blending mode etc.
 *
 * @arg style - The style to apply to the stream
 * @arg bbox - The bounding box being painted, used to control patterns
 */
void DrawContext::set_shape_style(SPStyle *style, Geom::Rect const &bbox)
{
    if (style->fill.set) {
        // Because soft masks negate the use of draw opacities, we must fold them in.
        std::optional<double> opacity;
        if (isSoftMask()) {
            opacity = style->fill_opacity;
        }
        if (auto color = _doc.get_paint(style->fill, bbox, opacity)) {
            _ctx.set_nonstroke(*color);
        }
    }
    if (style->stroke.set) {
        std::optional<double> opacity;
        if (isSoftMask()) {
            opacity = style->stroke_opacity;
        }
        if (auto color = _doc.get_paint(style->stroke, bbox, opacity)) {
            _ctx.set_stroke(*color);
        }
    }
    if (style->stroke_width.set) {
        //TODO: if (style->stroke_extensions.hairline) {
            //ink_cairo_set_hairline(_cr);
        _ctx.cmd_w(style->stroke_width.computed);
    }
    if (style->stroke_miterlimit.set) {
        _ctx.cmd_M(style->stroke_miterlimit.value);
    }
    if (style->stroke_linecap.set) {
        switch (style->stroke_linecap.computed) {
            case SP_STROKE_LINECAP_SQUARE:
                _ctx.cmd_J(CAPY_LC_PROJECTION);
                break;
            case SP_STROKE_LINECAP_ROUND:
                _ctx.cmd_J(CAPY_LC_ROUND);
                break;
            case SP_STROKE_LINECAP_BUTT:
                _ctx.cmd_J(CAPY_LC_BUTT);
                break;
            default:
                break;
        }
    }
    if (style->stroke_linejoin.set) {
        switch (style->stroke_linejoin.computed) {
            case SP_STROKE_LINEJOIN_ROUND:
                _ctx.cmd_j(CAPY_LJ_ROUND);
                break;
            case SP_STROKE_LINEJOIN_BEVEL:
                _ctx.cmd_j(CAPY_LJ_BEVEL);
                break;
            case SP_STROKE_LINEJOIN_MITER:
                _ctx.cmd_j(CAPY_LJ_MITER);
                break;
            default:
                break;
        }
    }
    if (style->stroke_dasharray.set) {
        double offset = style->stroke_dashoffset.computed;
        std::vector<double> values;
        for (auto val : style->stroke_dasharray.values) {
            values.push_back(val.computed);
        }
        if (values.size() > 1) {
            _ctx.cmd_d(values.data(), values.size(), offset);
        }
    }
}

} // namespace Inkscape::Extension::Internal::PdfBuilder
