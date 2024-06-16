// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Enums for CanvasItems.
 */
/*
 * Author:
 *   Tavmjong Bah
 *   Sanidhya Singh
 *
 * Copyright (C) 2020 Tavmjong Bah
 */

#ifndef SEEN_CANVAS_ITEM_ENUMS_H
#define SEEN_CANVAS_ITEM_ENUMS_H

namespace Inkscape {

enum CanvasItemColor {
    CANVAS_ITEM_PRIMARY,
    CANVAS_ITEM_SECONDARY,
    CANVAS_ITEM_TERTIARY
};

enum CanvasItemCtrlShape {
    CANVAS_ITEM_CTRL_SHAPE_SQUARE,
    CANVAS_ITEM_CTRL_SHAPE_DIAMOND,
    CANVAS_ITEM_CTRL_SHAPE_CIRCLE,
    CANVAS_ITEM_CTRL_SHAPE_HEXAGON,
    CANVAS_ITEM_CTRL_SHAPE_TRIANGLE,
    CANVAS_ITEM_CTRL_SHAPE_CROSS,
    CANVAS_ITEM_CTRL_SHAPE_PLUS,
    CANVAS_ITEM_CTRL_SHAPE_PIVOT,  // Fancy "plus"
    CANVAS_ITEM_CTRL_SHAPE_DARROW, // Double headed arrow.
    CANVAS_ITEM_CTRL_SHAPE_SARROW, // Double headed arrow, rotated (skew).
    CANVAS_ITEM_CTRL_SHAPE_CARROW, // Double headed curved arrow.
    CANVAS_ITEM_CTRL_SHAPE_SALIGN, // Side alignment.
    CANVAS_ITEM_CTRL_SHAPE_CALIGN, // Corner alignment.
    CANVAS_ITEM_CTRL_SHAPE_MALIGN, // Center (middle) alignment.
    CANVAS_ITEM_CTRL_SHAPE_TRIANGLE_ANGLED
};

enum CanvasItemCtrlType {
    CANVAS_ITEM_CTRL_TYPE_DEFAULT,
    CANVAS_ITEM_CTRL_TYPE_ADJ_HANDLE, // Stretch & Scale
    CANVAS_ITEM_CTRL_TYPE_ADJ_SKEW,
    CANVAS_ITEM_CTRL_TYPE_ADJ_ROTATE,
    CANVAS_ITEM_CTRL_TYPE_ADJ_CENTER,
    CANVAS_ITEM_CTRL_TYPE_ADJ_SALIGN,
    CANVAS_ITEM_CTRL_TYPE_ADJ_CALIGN,
    CANVAS_ITEM_CTRL_TYPE_ADJ_MALIGN,
    CANVAS_ITEM_CTRL_TYPE_ANCHOR,
    CANVAS_ITEM_CTRL_TYPE_POINT, // dot-like handle, indicator
    CANVAS_ITEM_CTRL_TYPE_ROTATE,
    CANVAS_ITEM_CTRL_TYPE_MARGIN,
    CANVAS_ITEM_CTRL_TYPE_CENTER,
    CANVAS_ITEM_CTRL_TYPE_SIZER,
    CANVAS_ITEM_CTRL_TYPE_SHAPER,
    CANVAS_ITEM_CTRL_TYPE_MARKER,
    CANVAS_ITEM_CTRL_TYPE_MESH,
    CANVAS_ITEM_CTRL_TYPE_LPE,
    CANVAS_ITEM_CTRL_TYPE_NODE_AUTO,
    CANVAS_ITEM_CTRL_TYPE_NODE_CUSP,
    CANVAS_ITEM_CTRL_TYPE_NODE_SMOOTH,
    CANVAS_ITEM_CTRL_TYPE_NODE_SYMMETRICAL,
    CANVAS_ITEM_CTRL_TYPE_INVISIPOINT,
    CANVAS_ITEM_CTRL_TYPE_GUIDE_HANDLE,
    CANVAS_ITEM_CTRL_TYPE_POINTER, // pointy, triangular handle
    CANVAS_ITEM_CTRL_TYPE_MOVE,
    //
    LAST_ITEM_CANVAS_ITEM_CTRL_TYPE
};

} // namespace Inkscape

#endif // SEEN_CANVAS_ITEM_ENUMS_H

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
