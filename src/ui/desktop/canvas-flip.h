/** @file
 * Enum describing canvas flips
 */
/*
 * Authors: see git history
 *
 * Copyright (C) 2025 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_CANVAS_FLIP_H
#define INKSCAPE_UI_CANVAS_FLIP_H

namespace Inkscape {

enum class CanvasFlip : int
{
    FLIP_NONE = 0,
    FLIP_HORIZONTAL = 1,
    FLIP_VERTICAL = 2
};

constexpr int operator&(CanvasFlip lhs, CanvasFlip rhs)
{
    return static_cast<int>(lhs) & static_cast<int>(rhs);
}
} // namespace Inkscape

#endif // INKSCAPE_UI_CANVAS_FLIP_H