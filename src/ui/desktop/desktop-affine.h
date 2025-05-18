// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Affine transformation of the desktop.
 */
/*
 * Authors: see git history
 *
 * Copyright (C) 2025 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_DESKTOP_AFFINE_H
#define INKSCAPE_UI_DESKTOP_AFFINE_H

#include <2geom/affine.h>
#include <2geom/point.h>
#include <2geom/transforms.h>

#include "ui/desktop/canvas-flip.h"

namespace Inkscape {

/** @class  DesktopAffine
 * @brief This simple class ensures that _w2d is always in sync with _rotation and _scale.
 *
 * We keep rotation and scale separate to avoid having to extract them from the affine.
 * With offset, this describes fully how to map the drawing to the window.
 * Future: merge offset as a translation in w2d.
 */
class DesktopAffine
{
public:
    Geom::Affine const &w2d() const { return _w2d; }

    Geom::Affine const &d2w() const { return _d2w; }

    void setScale(Geom::Scale scale)
    {
        _scale = scale;
        _update();
    }

    void addScale(Geom::Scale scale)
    {
        _scale *= scale;
        _update();
    }

    void setRotate(Geom::Rotate rotate)
    {
        _rotate = rotate;
        _update();
    }

    void setRotate(double rotate) { setRotate(Geom::Rotate{rotate}); }

    void addRotate(Geom::Rotate rotate)
    {
        _rotate *= rotate;
        _update();
    }

    void addRotate(double rotate) { addRotate(Geom::Rotate{rotate}); }

    void setFlip(CanvasFlip flip)
    {
        _flip = Geom::Scale();
        addFlip(flip);
    }

    bool isFlipped(CanvasFlip flip)
    {
        if ((flip & CanvasFlip::FLIP_HORIZONTAL) && Geom::are_near(_flip[0], -1)) {
            return true;
        }
        if ((flip & CanvasFlip::FLIP_VERTICAL) && Geom::are_near(_flip[1], -1)) {
            return true;
        }
        return false;
    }

    void addFlip(CanvasFlip flip)
    {
        if (flip & CanvasFlip::FLIP_HORIZONTAL) {
            _flip *= Geom::Scale(-1.0, 1.0);
        }
        if (flip & CanvasFlip::FLIP_VERTICAL) {
            _flip *= Geom::Scale(1.0, -1.0);
        }
        _update();
    }

    double getZoom() const { return _d2w.descrim(); }

    Geom::Rotate const &getRotation() const { return _rotate; }

    void setOffset(Geom::Point offset) { _offset = offset; }

    void addOffset(Geom::Point offset) { _offset += offset; }

    Geom::Point const &getOffset() { return _offset; }

private:
    void _update()
    {
        _d2w = _scale * _rotate * _flip;
        _w2d = _d2w.inverse();
    }

    Geom::Affine _w2d;    // Window to desktop
    Geom::Affine _d2w;    // Desktop to window
    Geom::Rotate _rotate; // Rotate part of _w2d
    Geom::Scale _scale;   // Scale part of _w2d, holds y-axis direction
    Geom::Scale _flip;    // Flip part of _w2d
    Geom::Point _offset;  // Point on canvas to align to (0,0) of window
};
} // namespace Inkscape

#endif // INKSCAPE_UI_DESKTOP_AFFINE_H
