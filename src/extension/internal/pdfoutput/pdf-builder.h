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

#ifndef EXTENSION_INTERNAL_PDF_CAPYPDF_H
#define EXTENSION_INTERNAL_PDF_CAPYPDF_H

#include "capypdf.hpp"

#include <2geom/2geom.h>
#include <optional>
#include <memory>

#include "style-enums.h"

class SPIPaint;
class SPStyle;
class SPPaintServer;
class SPGradientVector;

namespace Inkscape {
    class URI;
namespace Colors {
    class Color;
namespace Space {
    class AnySpace;
    class CMS;
}
}
}

inline auto const px2pt = Geom::Scale{72.0 / 96.0};

namespace Inkscape::Extension::Internal::PdfBuilder {

class DrawContext;
class PageContext;
class GroupContext;
class ShapeContext;

bool style_has_gradient_transparency(SPStyle *style);
bool gradient_has_transparency(SPPaintServer const *paint);

class Document
{
public:
    Document(char const *filename)
      : _gen(filename, _md)
    {
    }

    void add_page(PageContext &page);
    void write() { _gen.write(); }

    std::optional<CapyPDF_GraphicsStateId> get_child_graphics_state(SPStyle *style, std::optional<CapyPDF_TransparencyGroupId> sm);
    std::optional<CapyPDF_GraphicsStateId> get_shape_graphics_state(SPStyle *style, std::optional<CapyPDF_TransparencyGroupId> sm);

    capypdf::Color get_color(Colors::Color const &color, std::optional<double> opacity);
    std::optional<capypdf::Color> get_paint(SPIPaint const &paint, Geom::Rect const &bbox, std::optional<double> opacity);

    std::optional<CapyPDF_PatternId> get_pattern(SPPaintServer const *paint, Geom::Rect const &bbox, std::optional<double> opacity);
    std::optional<CapyPDF_FunctionId> get_gradient_function(SPGradientVector const &vector, std::optional<double> opacity, CapyPDF_DeviceColorspace *space);
    std::optional<CapyPDF_FunctionId> get_repeat_function(CapyPDF_FunctionId gradient, bool reflected, int from, int to);

    CapyPDF_DeviceColorspace get_default_colorspace();
    CapyPDF_DeviceColorspace get_colorspace(std::shared_ptr<Colors::Space::AnySpace> const &space);
    std::optional<CapyPDF_IccColorSpaceId> get_icc_profile(std::shared_ptr<Colors::Space::CMS> const &profile);

protected:
    friend class DrawContext;
    friend class PageContext;
    friend class GroupContext;

    capypdf::Generator &generator() { return _gen; }

private:

    capypdf::DocumentMetadata _md;
    capypdf::Generator _gen;

    std::map<std::string, CapyPDF_IccColorSpaceId> _icc_cache;
};

class DrawContext
{
public:
    DrawContext(Document &doc, capypdf::DrawContext ctx, bool soft_mask)
        : _ctx{std::move(ctx)}
        , _doc{doc}
        , _soft_mask{soft_mask}
    {}
    ~DrawContext() = default;

    void paint_group(GroupContext &child, Geom::Affine const &affine, SPStyle *style = nullptr);
    void paint_group(CapyPDF_TransparencyGroupId child_id, Geom::Affine const &affine, SPStyle *style = nullptr, std::optional<Geom::PathVector> clip = {}, std::optional<CapyPDF_TransparencyGroupId> soft_mask = {});
    void paint_shape(Geom::PathVector const &pathv, Geom::Rect const &bbox, SPStyle *style, std::optional<CapyPDF_TransparencyGroupId> soft_mask = {});
    void paint_raster(Inkscape::URI const &uri, SPStyle *style);

    bool render_pathvector(Geom::PathVector const &pathv);

    void set_shape_style(SPStyle *style, Geom::Rect const &bbox);

    Document &get_document() { return _doc; }
    bool isSoftMask() const { return _soft_mask; }

protected:
    void transform(Geom::Affine const &affine);

    capypdf::DrawContext _ctx;
    Document &_doc;

private:
    void render_path(Geom::Path const &path);
    void render_path_arc(Geom::EllipticalArc const &arc);

    bool _soft_mask = false;
};

class PageContext : public DrawContext
{
public:
    PageContext(Document &doc, Geom::Rect const &media_box);

    void set_pagebox(CapyPDF_Page_Box type, Geom::Rect const &size);
    void paint_drawing(CapyPDF_TransparencyGroupId drawing_id, Geom::Affine const &affine);

    void set_label(std::string label) {}

protected:
    friend class Document;

    void finalize();

private:
    Geom::Affine page_tr;
    capypdf::PageProperties page_props;
};

class GroupContext : public DrawContext
{
public:
    GroupContext(Document &doc, Geom::Rect const &clip, bool soft_mask = false);

    void set_transform(Geom::Affine const &tr);
    CapyPDF_TransparencyGroupId finalize();

private:
};

} // namespace Inkscape::Extension::Internal::PdfBuilder

#endif /* !EXTENSION_INTERNAL_PDF_CAPYPDF_H */

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
