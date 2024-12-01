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

#include "object/uri.h"

namespace Inkscape::Extension::Internal::PdfBuilder {

void Document::add_page(PageContext &page)
{
    page.finalize();
    _gen.add_page(page._ctx);
}

/**
 * Add a transform to the current context.
 */
void DrawContext::transform(Geom::Affine const &affine)
{
    if (affine != Geom::identity()) {
        _ctx.cmd_cm(affine[0], affine[1], affine[2], affine[3], affine[4], affine[5]);
    }
}

PageContext::PageContext(Document &doc, Geom::Rect const &media_box)
    : DrawContext(doc, doc.generator().new_page_context(), false)
    // 96 to 72 dpi plus flip y axis (for PDF) plus this page's translation in the SVG document.
    , page_tr(
              // The position of the page in the svg document
              Geom::Translate(-media_box.left(), -media_box.top())
              // Flip the Y-Axis because PDF is bottom-left
              * Geom::Affine(1.0, 0.0, 0.0, -1.0, 0.0, media_box.height())
              // Resize from SVG's 96dpi to PDF's 72dpi
              * px2pt)
{
    set_pagebox(CAPY_BOX_MEDIA, media_box);
}

void PageContext::set_pagebox(CapyPDF_Page_Box type, Geom::Rect const &size)
{
    // Page boxes are not affected by the cm transformations so must be transformed first
    auto box = size * page_tr;

    if (type == CAPY_BOX_MEDIA && !Geom::are_near(box.corner(0), {0.0, 0.0})) {
        // The specification technically allows non-zero media boxes, but lots of PDF
        // readers get very grumpy if you do this. Including Inkscape's own importer.
        g_warning("The media box must start at 0,0, found %f,%f", box.left(), box.top());
    }

    page_props.set_pagebox(type, box.left(), box.top(), box.right(), box.bottom());
};

void PageContext::paint_drawing(CapyPDF_TransparencyGroupId drawing_id, Geom::Affine const &affine)
{
    paint_group(drawing_id, affine * page_tr);
}

void PageContext::finalize()
{
    _ctx.set_custom_page_properties(page_props);
}

GroupContext::GroupContext(Document &doc, Geom::Rect const &clip, bool soft_mask)
    : DrawContext(doc, doc.generator().new_transparency_group_context(clip.left(), clip.bottom(), clip.right(), clip.top()), soft_mask)
{
    capypdf::TransparencyGroupProperties props;
    if (soft_mask) {
        props.set_CS(CAPY_DEVICE_CS_GRAY);
    } else {
        // Do groups have a color space?
    }
    props.set_I(true); // Isolate from the document
    props.set_K(false); // Do not knock out
    _ctx.set_transparency_group_properties(props);
}

void GroupContext::set_transform(Geom::Affine const &tr)
{
    _ctx.set_group_matrix(tr[0], tr[1], tr[2], tr[3], tr[4], tr[5]);
}

CapyPDF_TransparencyGroupId GroupContext::finalize()
{
    return _doc.generator().add_transparency_group(_ctx);
}

/**
 * Paint a child group at the requested location
 */
void DrawContext::paint_group(CapyPDF_TransparencyGroupId child_id, Geom::Affine const &affine, SPStyle *style, std::optional<Geom::PathVector> clip, std::optional<CapyPDF_TransparencyGroupId> soft_mask)
{
    _ctx.cmd_q();
    transform(affine);
    if (auto gsid = _doc.get_child_graphics_state(style, soft_mask)) {
        _ctx.cmd_gs(*gsid);
    }

    if (clip) {
        // TODO: get clip pathvector's fill rule
        render_pathvector(*clip);
        _ctx.cmd_W(); // or Wstar
        _ctx.cmd_n();
    }
    _ctx.cmd_Do(child_id);

    _ctx.cmd_Q();
}

/**
 * Paint a single shape path
 *
 * @param pathv - The Geom path we're going to draw
 * @param bbox - The bounding box for the drawn area
 * @param style - The drawing style (fill, stroke, etc)
 * @param soft_mask - The soft mask used for supporting transparent gradients in fills and strokes
 */
void DrawContext::paint_shape(Geom::PathVector const &pathv, Geom::Rect const &bbox, SPStyle *style, std::optional<CapyPDF_TransparencyGroupId> soft_mask)
{
    bool no_fill = style->fill.isNone() || style->fill_opacity.value == 0;
    bool no_stroke = style->stroke.isNone() || (!style->stroke_extensions.hairline && style->stroke_width.computed < 1e-9) ||
                     style->stroke_opacity.value == 0;
    
    if (no_fill && no_stroke) {
        return;
    }

    // Isolate the style of the shape
    _ctx.cmd_q();

    if (auto gsid = _doc.get_shape_graphics_state(style, soft_mask)) {
        _ctx.cmd_gs(*gsid);
    }
    set_shape_style(style, bbox);

    bool evenodd = style->fill_rule.computed == SP_WIND_RULE_EVENODD;
    auto layers = style->paint_order.get_layers();
    for (auto i = 0; i < 3; i++) {
        auto layer = layers[i];
        auto next = i < 2 ? layers[i + 1] : SP_CSS_PAINT_ORDER_NORMAL;

        if (layer == SP_CSS_PAINT_ORDER_FILL && next == SP_CSS_PAINT_ORDER_STROKE && !no_fill && !no_stroke) {
            // Stroke over fill, i.e. Both stroke and fill in order
            if (render_pathvector(pathv)) {
                if (evenodd) {
                    _ctx.cmd_bstar();
                } else {
                    _ctx.cmd_b();
                }
            } else { // Not closed path
                if (evenodd) {
                    _ctx.cmd_Bstar();
                } else {
                    _ctx.cmd_B();
                }
            }
            i++; // Stroke is already done, skip it.

        } else if (layer == SP_CSS_PAINT_ORDER_FILL && !no_fill) {
            // Fill only without stroke, either because it's only fill, or not in order
            render_pathvector(pathv);

            if (evenodd) {
                _ctx.cmd_fstar();
            } else {
                _ctx.cmd_f();
            }

        } else if (layer == SP_CSS_PAINT_ORDER_STROKE && !no_stroke) {
            // Stroke only without fill, either because it's only stroke, or not in order
            if (render_pathvector(pathv)) {
                _ctx.cmd_s();
            } else { // Not closed path
                _ctx.cmd_S();
            }

        } else if (layer == SP_CSS_PAINT_ORDER_MARKER && !no_stroke) {
            // TODO: Markers go here
        }
    }  

    // Deisolate the shape's style
    _ctx.cmd_Q();
}

/**
 * Draw the raster data stored in URI into the PDF context.
 */
void DrawContext::paint_raster(Inkscape::URI const &uri, SPStyle *style)
{
    //style.image_rendering.computed

    auto mime = uri.getMimeType();
    if (mime == "image/png") {
        g_warning("PNG loading goes here.");
    } else if (mime == "image/jpeg") {
        g_warning("JPEG loading goes here.");
    } else if (mime == "image/tiff") {
        g_warning("TIFF loading goes here.");
    } else {
        g_warning("Can not output '%s' into PDF, image type not supported.", mime.c_str());
    }
}


} // namespace Inkscape::Extension::Internal::PdfBuilder
