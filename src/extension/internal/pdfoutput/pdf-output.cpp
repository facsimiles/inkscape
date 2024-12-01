// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Provide a capypdf exporter for Inkscape
 *
 * Authors:
 *   Martin Owens <doctormo@geek-2.com>
 *
 * Copyright (C) 2024 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "pdf-output.h"

#include "bad-uri-exception.h"

#include "extension/system.h"
#include "extension/db.h"
#include "extension/output.h"

#include "display/drawing.h"
#include "display/curve.h"

#include "object/sp-item.h"
#include "object/sp-root.h"
#include "object/sp-page.h"
#include "object/sp-use.h"
#include "object/sp-image.h"
#include "object/sp-text.h"
#include "object/sp-symbol.h"
#include "object/sp-anchor.h"
#include "object/sp-flowtext.h"
#include "object/sp-marker.h"

#include "page-manager.h"
#include "document.h"
#include "style.h"

namespace Inkscape::Extension::Internal {

bool PdfOutput::check(Inkscape::Extension::Extension *)
{
    return Inkscape::Extension::db.get("org.inkscape.output.pdf.capypdf");
}

/**
 * Render any type of item into a transparency group.
 */
std::optional<CapyPDF_TransparencyGroupId> PdfOutput::render_item(SPItem const *item, std::string cache_key)
{
    if (item->isHidden()) {
        return {};
    }

    // Groups require pre-defined clipping regions which must not be transformed
    auto const bbox = item->visualBounds(Geom::identity(), true, false, true);
    if (!bbox) {
        return {};
    }

    // Items are cached so they can be reused
    cache_key += item->getId();

    if (auto const it = _item_cache.find(cache_key); it != _item_cache.end()) {
        return it->second;
    }

    if (auto anchor = cast<SPAnchor>(item)) {
        // TODO: Render anchors out as bounding boxes, plus contents, without a new group context.
    }

    // Draw item on a group so a mask, clip-path, or opacity can be applied to it globally.
    auto group_ctx = PdfBuilder::GroupContext(*_pdf, *bbox);
    group_ctx.set_transform(item->transform);

    if (auto shape = cast<SPShape>(item)) {
        render_item_shape(group_ctx, shape);
    } else if (auto use = cast<SPUse>(item)) {
        if (auto item = use->get_original()) {
            // TODO: Styles need to be propegated here
            // There's two possible ways of getting styles to propegate, generate a new object for every use, or generate an id which  depends on the "holes" plus the contents of the context style or "blocks". If they fit, reuse, if they don't, make a new one.
            paint_item(group_ctx, item);
        }
    } else if (auto text = cast<SPText>(item)) {
        //sp_text_render(group_ctx, text);
    } else if (auto flowtext = cast<SPFlowtext>(item)) {
        //sp_flowtext_render(group_ctx, flowtext);
    } else if (auto image = cast<SPImage>(item)) {
        render_item_raster(group_ctx, image);
    } else if (auto group = cast<SPGroup>(item)) {
        // Because every group is a reusable transparency group in PDF, we can just ask Symbols to be painted in place as groups.
        // SPSymbol, SPRoot
        // Render children in the group
        for (auto &obj : group->children) {
            if (auto child_item = cast<SPItem>(&obj)) {
                paint_item(group_ctx, child_item);
            }
        }
        // TODO: Add layers as OCG's or Optional Content Groups
    }

    // We save the group_ctx id so it can be painted in any other contexts (symbols, clones, markers, etc)
    auto const item_id = group_ctx.finalize();
    _item_cache[cache_key] = item_id;
    return item_id;
}

/**
 * Paint the item into the given group context
 */
void PdfOutput::paint_item(PdfBuilder::GroupContext &ctx, SPItem const *item, Geom::Affine const &transform)
{
    if (auto item_id = render_item(item)) {
        // Each reused transparency group has to re-specify it's transform and opacity settings
        // since PDF applies properties from the outside of the group being drawn.
        ctx.paint_group(*item_id, transform, item->style, item->getClipPathVector());
    }
}

void PdfOutput::render_item_raster(PdfBuilder::DrawContext &ctx, SPImage const *image)
{
    try {
        ctx.paint_raster(image->getURI(), image->style);
    } catch (Inkscape::BadURIException &e) {
        g_warning("Couldn't output image in PDF: %s", e.what());
    }
}

void PdfOutput::render_item_shape(PdfBuilder::DrawContext &ctx, SPShape const *shape)
{
    if (!shape->curve()) {
        return;
    }

    auto const &pathv = shape->curve()->get_pathvector();
    if (pathv.empty()) {
        return;
    }

    auto bbox = shape->visualBounds(Geom::identity(), true, false, true); 

    // If needed, run the rendering process for gradient transparencies
    std::optional<CapyPDF_TransparencyGroupId> mask;
    if (PdfBuilder::style_has_gradient_transparency(shape->style)) {
        auto mask_ctx = PdfBuilder::GroupContext(*_pdf, *bbox, true);
        mask_ctx.paint_shape(pathv, *bbox, shape->style);
        mask = mask_ctx.finalize();
    }
    ctx.paint_shape(pathv, *bbox, shape->style, mask);
}

/**
 * Save the PDF file
 */
void PdfOutput::save(Inkscape::Extension::Output *mod, SPDocument *doc, char const *filename)
{
    _pdf.emplace(filename);
    _item_cache.clear();

    auto guard = scope_exit([&] {
        _pdf.reset();
        _item_cache.clear();
    });

    // Step 1. Render EVERYTHING in the document out to a single PDF TransparencyGroup
    auto drawing_id = render_item(doc->getRoot());

    if (!drawing_id) {
        std::cerr << "Nothing written to drawing context for PDF output!\n";
        return;
    }

    // Step 2. Enable pages for this document. It SHOULD be a copy by this stage
    auto &pm = doc->getPageManager();
    pm.enablePages();

    // Step 3. Tell the PDF where to draw that whole plate on the PDF pages
    for (auto const &svg_page : pm.getPages()) {
        auto pdf_page = PdfBuilder::PageContext(*_pdf, svg_page->getDocumentBleed());

        if (!svg_page->isBarePage()) {
            pdf_page.set_pagebox(CAPY_BOX_BLEED, svg_page->getDocumentRect());
            pdf_page.set_pagebox(CAPY_BOX_TRIM, svg_page->getDocumentRect());
            pdf_page.set_pagebox(CAPY_BOX_ART, svg_page->getDocumentMargin());
        }

        if (auto label = svg_page->label()) {
            pdf_page.set_label(label);
        }

        pdf_page.paint_drawing(*drawing_id, doc->getRoot()->c2p);
        _pdf->add_page(pdf_page);
    }

    _pdf->write();
}

#include "../clear-n_.h"

void PdfOutput::init()
{
    // clang-format off
    Inkscape::Extension::build_from_mem(
        "<inkscape-extension xmlns=\"" INKSCAPE_EXTENSION_URI "\">\n"
            "<name>Portable Document Format</name>\n"
            "<id>org.inkscape.output.pdf.capypdf</id>\n"
            "<param name=\"PDFversion\" gui-text=\"" N_("Restrict to PDF version:") "\" type=\"optiongroup\" appearance=\"combo\" >\n"
                "<option value='PDF-1.5'>" N_("PDF 1.5") "</option>\n"
                "<option value='PDF-1.4'>" N_("PDF 1.4") "</option>\n"
            "</param>\n"
            "<param name=\"blurToBitmap\" gui-text=\"" N_("Rasterize filter effects") "\" type=\"bool\">true</param>\n"
            "<param name=\"resolution\" gui-text=\"" N_("Resolution for rasterization (dpi):") "\" type=\"int\" min=\"1\" max=\"10000\">96</param>\n"
            "<output is_exported='true' priority='4'>\n"
                "<extension>.pdf</extension>\n"
                "<mimetype>application/pdf</mimetype>\n"
                "<filetypename>PDF (*.pdf)</filetypename>\n"
                "<filetypetooltip>Good PDF File</filetypetooltip>\n"
            "</output>\n"
        "</inkscape-extension>", std::make_unique<PdfOutput>());
    // clang-format on
}

} // namespace Inkscape::Extension::Internal
