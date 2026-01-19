// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Drawing patterns in cairo
 *//*
 * Copyright (C) 2026 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_INKSCAPE_RENDERER_CONTEXT_PATTERN_H
#define SEEN_INKSCAPE_RENDERER_CONTEXT_PATTERN_H

#include <memory>
#include <2geom/forward.h>
#include <cairomm/pattern.h>

#include "object/sp-gradient-spread.h"

#include "colors/forward.h"

namespace Inkscape::Renderer {

class Surface;

class Pattern
{
protected:
    Pattern(std::shared_ptr<Colors::Space::AnySpace> const &space)
        : _color_space(space)
    {}

    std::vector<Cairo::RefPtr<Cairo::Pattern>> _pts;
    std::shared_ptr<Colors::Space::AnySpace> _color_space;

public:
    Pattern(Surface const &surface);

    auto &getCairoPatterns() const { return _pts; }
    auto getColorSpace() const { return _color_space; }

    void setFilter(Cairo::SurfacePattern::Filter filter);
    void setExtend(Cairo::Pattern::Extend extend);
    void setExtend(SPGradientSpread spread);
    void setMatrix(Geom::Affine const &m);
    void setMatrixBox(Geom::Affine const &m, Geom::OptRect const &bbox);
    void setDither(bool enable);
    void addColorStop(double pos, Colors::Color solid_color);
};

class SolidColorPattern : public Pattern
{
public:
    SolidColorPattern(Colors::Color solid_color);
};

class LinearGradientPattern : public Pattern
{
public:
    LinearGradientPattern(std::shared_ptr<Colors::Space::AnySpace> const &space, double x0, double y0, double x1, double y1);
};

class RadialGradientPattern : public Pattern
{
public:
    RadialGradientPattern(std::shared_ptr<Colors::Space::AnySpace> const &space, double cx0, double cy0, double cr0, double cx1, double cy1, double cr1);
};

class MeshGradientPattern : public Pattern
{
public:
    MeshGradientPattern(std::shared_ptr<Colors::Space::AnySpace> const &space);

    void beginPatch();
    void endPatch();
    void moveTo(Geom::Point const &p);
    void lineTo(Geom::Point const &p);
    void curveTo(Geom::Point const &p0, Geom::Point const &p1, Geom::Point const &p2);
    void setControlPoint(int corner, Geom::Point const &p);
    void setCornerColor(int corner, Colors::Color color);

};

} // end namespace Inkscape::Renderer

#endif // !SEEN_INKSCAPE_RENDERER_CONTEXT_PATTERN_H

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
