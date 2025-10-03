// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SEEN_CANVAS_ITEM_SQUIGGLE_H
#define SEEN_CANVAS_ITEM_SQUIGGLE_H

/**
 * A class to represent squiggles
 */

#include <cstdint>
#include <2geom/pathvector.h>
#include <2geom/point.h>

#include "canvas-item.h"

namespace Inkscape {

class CanvasItemSquiggle : public CanvasItem
{
public:
    CanvasItemSquiggle(CanvasItemGroup *group, Geom::Point const &start, Geom::Point const &end, uint32_t color = 0xff0000ff);
    CanvasItemSquiggle(CanvasItemGroup *group, std::vector<Geom::Point> const &points, uint32_t color = 0xff0000ff);

    // Properties
    void set_points(Geom::Point start, Geom::Point end);
    void set_points(std::vector<Geom::Point> const &points);
    void set_squiggle_params(double amplitude = 3.5, double wavelength = 8.0, double sample_dt = 0.02);
    void set_color(uint32_t color);

protected:
    ~CanvasItemSquiggle() override = default;

    void _update(bool propagate) override;
    void _render(CanvasItemBuffer &buf) const override;

private:
    // Geometry
    Geom::Point _start;
    Geom::Point _end;

    std::vector<Geom::Point> _points;

    uint32_t _color;
    double _amplitude;   // Amplitude of the squiggle in canvas units
    double _wavelength;  // Wavelength of the squiggle in canvas units
    double _sample_dt;   // Sampling step for drawing the squiggle

    Geom::PathVector _squiggle_path;

    // Rebuilding
    void _rebuild_squiggle();
};

} // namespace Inkscape

#endif // SEEN_CANVAS_ITEM_SQUIGGLE_H