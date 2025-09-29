// SPDX-License-Identifier: GPL-2.0-or-later

#include "canvas-item-squiggle.h"

#include <cmath>

#include "display/cairo-utils.h"

namespace Inkscape {

CanvasItemSquiggle::CanvasItemSquiggle(CanvasItemGroup *group, Geom::Point const &start, Geom::Point const &end, uint32_t color)
    : CanvasItem(group)
    , _start(start)
    , _end(end)
    , _color(color)
{
    _name = "CanvasItemSquiggle";
    _pickable = false;
    request_update();
}

void CanvasItemSquiggle::set_points(Geom::Point start, Geom::Point end)
{
    if (_start != start || _end != end) {
        _start = start;
        _end = end;
        request_update();
    }
}

void CanvasItemSquiggle::set_color(uint32_t color)
{
    if (_color != color) {
        _color = color;
        request_redraw();
    }
}

void CanvasItemSquiggle::_rebuild_squiggle()
{
    // Transform start and end from document to canvas units
    Geom::Affine aff = affine();
    Geom::Point s = _start * aff;
    Geom::Point e = _end * aff;

    // Minimum length in canvas units to draw squiggle
    constexpr double min_canvas_len = 4.0;
    double len = Geom::L2(e - s);

    _squiggle_path.clear();

    if (len < min_canvas_len) {
        return;
    }

    // Parameters for squiggle in canvas units (screen size)
    double amplitude = 3.5;
    double wavelength = 8.0;
    int n = std::max(1, int(len / wavelength));
    double step = len / n;

    Geom::Point dir = (e - s) / len;
    Geom::Point perp(-dir[1], dir[0]);
    Geom::Path path;
    path.start(s);

    for (int i = 1; i <= n; ++i) {
        double t = i * step;
        Geom::Point next = s + dir * t;
        double sign = (i % 2 == 0) ? 1.0 : -1.0;
        Geom::Point ctrl = s + dir * (t - step / 2) + perp * (amplitude * sign);
        path.appendNew<Geom::CubicBezier>(ctrl, ctrl, next);
    }
    _squiggle_path.push_back(path);
}

void CanvasItemSquiggle::_update(bool)
{
    request_redraw();

    _rebuild_squiggle();

    // Set bounds (just a box around the squiggle)
    Geom::Rect bounds_doc(_start, _end);
    bounds_doc.expandBy(5.0); // Expand by 5 canvas units, convert to doc units
    _bounds = bounds_doc;

    *_bounds *= affine();

    request_redraw();
}

void CanvasItemSquiggle::_render(CanvasItemBuffer &buf) const
{
    if (_squiggle_path.empty()) {
        return;
    }

    buf.cr->save();

    buf.cr->set_tolerance(0.5);
    buf.cr->begin_new_path();

    // Draw in screen coordinates but no affine transformation cause it is already in canvas coordinates
    feed_pathvector_to_cairo(buf.cr->cobj(), _squiggle_path, Geom::Affine(), buf.rect, true, 0);

    ink_cairo_set_source_color(buf.cr, Colors::Color(_color));
    buf.cr->set_line_width(1.5);
    buf.cr->stroke();

    buf.cr->restore();
}

} // namespace Inkscape