// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Output an SVG to a PDF using capypdf
 *
 * Authors:
 *   Martin Owens <doctormo@geek-2.com?
 *
 * Copyright (C) 2024 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef EXTENSION_INTERNAL_PDFOUTPUT_H
#define EXTENSION_INTERNAL_PDFOUTPUT_H

#include "extension/implementation/implementation.h"

#include "pdf-builder.h"

class SPGroup;
class SPImage;
class SPItem;
class SPRoot;
class SPShape;
class SPUse;

namespace Inkscape::Extension::Internal {

class PdfOutput : public Inkscape::Extension::Implementation::Implementation
{
public:
    bool check(Inkscape::Extension::Extension *module) override;
    void save(Inkscape::Extension::Output *mod,
              SPDocument *doc,
              char const *filename) override;
    static void init();

private:
    std::optional<CapyPDF_TransparencyGroupId> render_item(SPItem const *item, std::string cache_key = "");
    void paint_item(PdfBuilder::GroupContext &ctx, SPItem const *item, Geom::Affine const &tr = Geom::identity());

    void render_item_shape(PdfBuilder::DrawContext &ctx, SPShape const *shape);
    void render_item_raster(PdfBuilder::DrawContext &ctx, SPImage const *image);

    std::optional<PdfBuilder::Document> _pdf;
    std::map<std::string, CapyPDF_TransparencyGroupId> _item_cache;
};

} // namespace Inkscape::Extension::Internal

#endif /* !EXTENSION_INTERNAL_PDFOUTPUT_H */

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
