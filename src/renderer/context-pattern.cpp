// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Drawing patterns in cairo
 *//*
 * Copyright (C) 2026 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "context-pattern.h"
#include "colors/color.h"
#include "colors/spaces/base.h"

#include "surface.h"
#include "context.h"

namespace Inkscape::Renderer {

Pattern::Pattern(Surface const &surface)
    : _color_space(surface.getColorSpace())
{
    for (auto &s : surface.getCairoSurfaces()) {
        _pts.emplace_back(Cairo::SurfacePattern::create(s));
    }
}

void Pattern::setFilter(Cairo::SurfacePattern::Filter filter)
{
    for (auto &pt : _pts) {
        if (auto sp = dynamic_cast<Cairo::SurfacePattern *>(&*pt)) {
            sp->set_filter(filter);
        }
    }
}

void Pattern::setExtend(Cairo::Pattern::Extend extend)
{
    for (auto &pt : _pts) {
        pt->set_extend(extend);
    }
}

void Pattern::setExtend(SPGradientSpread spread)
{
    switch (spread) {
        case SP_GRADIENT_SPREAD_REFLECT:
            setExtend(Cairo::Pattern::Extend::REFLECT);
            break;
        case SP_GRADIENT_SPREAD_REPEAT:
            setExtend(Cairo::Pattern::Extend::REPEAT);
            break;
        case SP_GRADIENT_SPREAD_PAD:
        default:
            setExtend(Cairo::Pattern::Extend::PAD);
            break;
    }
}

void Pattern::setMatrix(Geom::Affine const &m)
{
    for (auto &pt : _pts) {
        pt->set_matrix(geom_to_cairo(m));
    }
}

void Pattern::setMatrixBox(Geom::Affine const &transform, Geom::OptRect const &bbox)
{
    auto gs2user = transform;
    if (bbox) {
        auto bbox2user = Geom::Affine(bbox->width(), 0, 0, bbox->height(), bbox->left(), bbox->top());
        gs2user *= bbox2user;
    }
    setMatrix(gs2user.inverse());
}


void Pattern::setDither(bool enabled)
{
#if CAIRO_VERSION >= CAIRO_VERSION_ENCODE(1, 18, 0)
    for (auto &pt : _pts) {
        cairo_pattern_set_dither(pt->cobj(), enabled ? CAIRO_DITHER_BEST : CAIRO_DITHER_NONE);
    }
#endif
}

void Pattern::addColorStop(double offset, Colors::Color solid_color)
{
    solid_color.convert(_color_space);
    auto c = solid_color.getValues();
    c.resize(c.size() + 3); // Blind pad

    for (unsigned i = 0, j = 0; i < _pts.size(); i++, j+=3) {
        cairo_pattern_add_color_stop_rgba(_pts[i]->cobj(), offset, c[j], c[j+1], c[j+2], solid_color.getOpacity());
    }
}

SolidColorPattern::SolidColorPattern(Colors::Color solid_color)
    : Pattern(solid_color.getSpace())
{
    auto a = 0.0;
    bool has_a = solid_color.hasOpacity();
    auto c = solid_color.getValues();

    if (has_a) {
        // Steal alpha
        std::swap(a, c.back());
    }
    c.resize(c.size() + 3); // Blind pad

    for (unsigned i = 0; i < solid_color.size(); i += 3) {
        if (has_a) {
            _pts.emplace_back(Cairo::SolidPattern::create_rgba(c[i], c[i+1], c[i+2], a));
        } else {
            _pts.emplace_back(Cairo::SolidPattern::create_rgb(c[i], c[i+1], c[i+2]));
        }
    }
}

LinearGradientPattern::LinearGradientPattern(std::shared_ptr<Colors::Space::AnySpace> const &space, double x0, double y0, double x1, double y1)
    : Pattern(space)
{
    for (unsigned i = 0; i < space->getComponentCount(); i += 3) {
        _pts.emplace_back(Cairo::LinearGradient::create(x0, y0, x1, y1));
    }
}

RadialGradientPattern::RadialGradientPattern(std::shared_ptr<Colors::Space::AnySpace> const &space, double cx0, double cy0, double cr0, double cx1, double cy1, double cr1)
    : Pattern(space)
{
    for (unsigned i = 0; i < space->getComponentCount(); i += 3) {
        _pts.emplace_back(Cairo::RadialGradient::create(cx0, cy0, cr0, cx1, cy1, cr1));
    }
}

MeshGradientPattern::MeshGradientPattern(std::shared_ptr<Colors::Space::AnySpace> const &space)
    : Pattern(space)
{
    for (unsigned i = 0; i < space->getComponentCount(); i += 3) {
        // C++ API is unavailable in cairomm 1.16
        _pts.emplace_back(new Cairo::Pattern(cairo_pattern_create_mesh()));
    }
}

void MeshGradientPattern::beginPatch()
{
    for (auto &pt : _pts) {
        cairo_mesh_pattern_begin_patch(pt->cobj());
    }
}

void MeshGradientPattern::endPatch()
{
    for (auto &pt : _pts) {
        cairo_mesh_pattern_end_patch(pt->cobj());
    }
}

void MeshGradientPattern::moveTo(Geom::Point const &p)
{
    for (auto &pt : _pts) {
        cairo_mesh_pattern_move_to(pt->cobj(), p.x(), p.y());
    }
}

void MeshGradientPattern::lineTo(Geom::Point const &p)
{
    for (auto &pt : _pts) {
        cairo_mesh_pattern_line_to(pt->cobj(), p.x(), p.y());
    }
}

void MeshGradientPattern::curveTo(Geom::Point const &p0, Geom::Point const &p1, Geom::Point const &p2)
{
    for (auto &pt : _pts) {
        cairo_mesh_pattern_curve_to(pt->cobj(), p0.x(), p0.y(), p1.x(), p1.y(), p2.x(), p2.y());
    }
}

void MeshGradientPattern::setControlPoint(int point_num, Geom::Point const &p)
{
    for (auto &pt : _pts) {
        cairo_mesh_pattern_set_control_point(pt->cobj(), point_num, p.x(), p.y());
    }
}

void MeshGradientPattern::setCornerColor(int corner, Colors::Color color)
{
    color.convert(_color_space);
    auto c = color.getValues();
    c.resize(c.size() + 3); // Blind pad

    for (unsigned i = 0, j = 0; i < _pts.size(); i++, j+=3) {
        cairo_mesh_pattern_set_corner_color_rgba(_pts[i]->cobj(), corner, c[j], c[j+1], c[j+2], color.getOpacity());
    }
}

} // end namespace Inkscape

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
