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

    // Properties
    void set_points(Geom::Point start, Geom::Point end);
    void set_color(uint32_t color);

protected:
    ~CanvasItemSquiggle() override = default;

    void _update(bool propagate) override;
    void _render(CanvasItemBuffer &buf) const override;

private:
    // Geometry
    Geom::Point _start;
    Geom::Point _end;

    uint32_t _color;

    Geom::PathVector _squiggle_path;

    // Rebuilding
    void _rebuild_squiggle();
};

} // namespace Inkscape

#endif // SEEN_CANVAS_ITEM_SQUIGGLE_H