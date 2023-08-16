// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * A class to represent a control node.
 */

/*
 * Author:
 *   Tavmjong Bah
 *
 * Copyright (C) 2020 Tavmjong Bah
 *
 * Rewrite of SPCtrl
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <2geom/transforms.h>
// #include "3rdparty/libcroco/src/libcroco.h"
#include "3rdparty/libcroco/src/cr-selector.h"
#include "3rdparty/libcroco/src/cr-doc-handler.h"
#include "3rdparty/libcroco/src/cr-string.h"
#include "3rdparty/libcroco/src/cr-term.h"
#include "3rdparty/libcroco/src/cr-parser.h"
#include "3rdparty/libcroco/src/cr-rgb.h"
#include "3rdparty/libcroco/src/cr-utils.h"

#include "canvas-item-ctrl.h"
#include "helper/geom.h"

#include "io/resource.h"
#include "io/sys.h"

#include "preferences.h" // Default size. 
#include "display/cairo-utils.h" // argb32_from_rgba()

#include "ui/widget/canvas.h"

namespace Inkscape {

//Declaration of static members
InitLock CanvasItemCtrl::_parsed;
std::unordered_map<Handle, std::unordered_map<int, std::shared_ptr<uint32_t[]>>> CanvasItemCtrl::handle_cache;
std::unordered_map<Handle, HandleStyle *> CanvasItemCtrl::handle_styles;

/**
 * Create a null control node.
 */
CanvasItemCtrl::CanvasItemCtrl(CanvasItemGroup *group)
    : CanvasItem(group)
{
    _name = "CanvasItemCtrl:Null";
    _pickable = true; // Everybody gets events from this class!
}

/**
 * Create a control ctrl. Shape auto-set by type.
 */
CanvasItemCtrl::CanvasItemCtrl(CanvasItemGroup *group, CanvasItemCtrlType type)
    : CanvasItem(group)
{
    _handle._type = type;
    _name = "CanvasItemCtrl:Type_" + std::to_string(_handle._type);
    _pickable = true; // Everybody gets events from this class!

    // Use _type to set default values:
    set_shape_default();
    set_size_default();
}

/**
 * Create a control ctrl. Point is in document coordinates.
 */
CanvasItemCtrl::CanvasItemCtrl(CanvasItemGroup *group, CanvasItemCtrlType type, Geom::Point const &p)
    : CanvasItemCtrl(group, type)
{
    _position = p;
    request_update();
}

/**
 * Create a control ctrl.
 */
CanvasItemCtrl::CanvasItemCtrl(CanvasItemGroup *group, CanvasItemCtrlShape shape)
    : CanvasItem(group)
    , _shape(shape)
{
    _name = "CanvasItemCtrl:Shape_" + std::to_string(_shape);
    _pickable = true; // Everybody gets events from this class!
}

/**
 * Create a control ctrl. Point is in document coordinates.
 */
CanvasItemCtrl::CanvasItemCtrl(CanvasItemGroup *group, CanvasItemCtrlShape shape, Geom::Point const &p)
    : CanvasItemCtrl(group, shape)
{
    _position = p;
}

/**
 * Set the position. Point is in document coordinates.
 */
void CanvasItemCtrl::set_position(Geom::Point const &position)
{
    // std::cout << "CanvasItemCtrl::set_ctrl: " << _name << ": " << position << std::endl;
    defer([ = ] {
        if (_position == position) return;
        _position = position;
        request_update();
    });
}

/**
 * Returns distance between point in canvas units and position of ctrl.
 */
double CanvasItemCtrl::closest_distance_to(Geom::Point const &p) const
{
    // TODO: Different criteria for different shapes.
    return Geom::distance(p, _position * affine());
}

/**
 * If tolerance is zero, returns true if point p (in canvas units) is inside bounding box,
 * else returns true if p (in canvas units) is within tolerance (canvas units) distance of ctrl.
 * The latter assumes ctrl center anchored.
 */
bool CanvasItemCtrl::contains(Geom::Point const &p, double tolerance)
{
    // TODO: Different criteria for different shapes.
    if (!_bounds) {
        return false;
    }
    if (tolerance == 0) {
        return _bounds->interiorContains(p);
    }
    else {
        return closest_distance_to(p) <= tolerance;
    }
}

static auto angle_of(Geom::Affine const &affine)
{
    return std::atan2(affine[1], affine[0]);
}

void CanvasItemCtrl::set_fill(uint32_t fill)
{
    defer([ = ] {
        if (_fill == fill) return;
        _fill = fill;
        _built.reset();
        request_redraw();
    });
}

void CanvasItemCtrl::set_stroke(uint32_t stroke)
{
    defer([ = ] {
        if (_stroke == stroke) return;
        _stroke = stroke;
        _built.reset();
        request_redraw();
    });
}

void CanvasItemCtrl::set_shape(CanvasItemCtrlShape shape)
{
    defer([ = ] {
        if (_shape == shape) return;
        _shape = shape;
        _built.reset();
        request_update(); // Geometry could change
    });
}

void CanvasItemCtrl::set_shape_default()
{
    switch (_handle._type) {
    case CANVAS_ITEM_CTRL_TYPE_ADJ_HANDLE:
        _shape = CANVAS_ITEM_CTRL_SHAPE_DARROW;
        break;

    case CANVAS_ITEM_CTRL_TYPE_ADJ_SKEW:
        _shape = CANVAS_ITEM_CTRL_SHAPE_SARROW;
        break;

    case CANVAS_ITEM_CTRL_TYPE_ADJ_ROTATE:
        _shape = CANVAS_ITEM_CTRL_SHAPE_CARROW;
        break;

    case CANVAS_ITEM_CTRL_TYPE_ADJ_CENTER:
        _shape = CANVAS_ITEM_CTRL_SHAPE_PIVOT;
        break;

    case CANVAS_ITEM_CTRL_TYPE_ADJ_SALIGN:
        _shape = CANVAS_ITEM_CTRL_SHAPE_SALIGN;
        break;

    case CANVAS_ITEM_CTRL_TYPE_ADJ_CALIGN:
        _shape = CANVAS_ITEM_CTRL_SHAPE_CALIGN;
        break;

    case CANVAS_ITEM_CTRL_TYPE_ADJ_MALIGN:
        _shape = CANVAS_ITEM_CTRL_SHAPE_MALIGN;
        break;

    case CANVAS_ITEM_CTRL_TYPE_NODE_AUTO:
    case CANVAS_ITEM_CTRL_TYPE_ROTATE:
    case CANVAS_ITEM_CTRL_TYPE_MARGIN:
        _shape = CANVAS_ITEM_CTRL_SHAPE_CIRCLE;
        break;

    case CANVAS_ITEM_CTRL_TYPE_CENTER:
        _shape = CANVAS_ITEM_CTRL_SHAPE_PLUS;
        break;

    case CANVAS_ITEM_CTRL_TYPE_SHAPER:
    case CANVAS_ITEM_CTRL_TYPE_LPE:
    case CANVAS_ITEM_CTRL_TYPE_NODE_CUSP:
        _shape = CANVAS_ITEM_CTRL_SHAPE_DIAMOND;
        break;

    case CANVAS_ITEM_CTRL_TYPE_POINT:
        _shape = CANVAS_ITEM_CTRL_SHAPE_CROSS;
        break;

    default:
        _shape = CANVAS_ITEM_CTRL_SHAPE_SQUARE;
    }
}

void CanvasItemCtrl::set_mode(CanvasItemCtrlMode mode)
{
    defer([ = ] {
        if (_mode == mode) return;
        _mode = mode;
        _built.reset();
        request_update();
    });
}

void CanvasItemCtrl::set_pixbuf(Glib::RefPtr<Gdk::Pixbuf> pixbuf)
{
    defer([ =, pixbuf = std::move(pixbuf)]() mutable {
        if (_pixbuf != pixbuf) return;
        _pixbuf = std::move(pixbuf);
        _width = _pixbuf->get_width();
        _height = _pixbuf->get_height();
        _built.reset();
        request_update();
    });
}

// Nominally width == height == size except possibly for pixmaps.
void CanvasItemCtrl::set_size(int size)
{
    defer([ = ] {
        if (_pixbuf)
        {
            // std::cerr << "CanvasItemCtrl::set_size: Attempting to set size on pixbuf control!" << std::endl;
            return;
        }
        if (_width == size + _extra && _height == size + _extra) return;
        _width  = size + _extra;
        _height = size + _extra;
        _built.reset();
        request_update(); // Geometry change
    });
}

void CanvasItemCtrl::set_size_via_index(int size_index)
{
    // Size must always be an odd number to center on pixel.

    if (size_index < 1 || size_index > 15) {
        std::cerr << "CanvasItemCtrl::set_size_via_index: size_index out of range!" << std::endl;
        size_index = 3;
    }

    int size = 0;
    switch (_handle._type) {
    case CANVAS_ITEM_CTRL_TYPE_ADJ_HANDLE:
    case CANVAS_ITEM_CTRL_TYPE_ADJ_SKEW:
        size = size_index * 2 + 7;
        break;

    case CANVAS_ITEM_CTRL_TYPE_ADJ_ROTATE:
    case CANVAS_ITEM_CTRL_TYPE_ADJ_CENTER:
        size = size_index * 2 + 9; // 2 larger than HANDLE/SKEW
        break;

    case CANVAS_ITEM_CTRL_TYPE_ADJ_SALIGN:
    case CANVAS_ITEM_CTRL_TYPE_ADJ_CALIGN:
    case CANVAS_ITEM_CTRL_TYPE_ADJ_MALIGN:
        size = size_index * 4 + 5; // Needs to be larger to allow for rotating.
        break;

    case CANVAS_ITEM_CTRL_TYPE_POINT:
    case CANVAS_ITEM_CTRL_TYPE_ROTATE:
    case CANVAS_ITEM_CTRL_TYPE_MARGIN:
    case CANVAS_ITEM_CTRL_TYPE_CENTER:
    case CANVAS_ITEM_CTRL_TYPE_SIZER:
    case CANVAS_ITEM_CTRL_TYPE_SHAPER:
    case CANVAS_ITEM_CTRL_TYPE_LPE:
    case CANVAS_ITEM_CTRL_TYPE_NODE_AUTO:
    case CANVAS_ITEM_CTRL_TYPE_NODE_CUSP:
        size = size_index * 2 + 5;
        break;

    case CANVAS_ITEM_CTRL_TYPE_NODE_SMOOTH:
    case CANVAS_ITEM_CTRL_TYPE_NODE_SYMETRICAL:
        size = size_index * 2 + 3;
        break;

    case CANVAS_ITEM_CTRL_TYPE_INVISIPOINT:
        size = 1;
        break;

    case CANVAS_ITEM_CTRL_TYPE_ANCHOR: // vanishing point for 3D box and anchor for pencil
        size = size_index * 2 + 1;
        break;

    case CANVAS_ITEM_CTRL_TYPE_DEFAULT:
        size = size_index * 2 + 1;
        break;

    default:
        g_warning("set_size_via_index: missing case for handle type: %d", static_cast<int>(_handle._type));
        size = size_index * 2 + 1;
        break;
    }

    // std::cout<<_shape<<" - "<<size<<std::endl;
    set_size(size);
}

void CanvasItemCtrl::set_size_default()
{
    int size = Preferences::get()->getIntLimited("/options/grabsize/value", 3, 1, 15);
    set_size_via_index(size);
}

void CanvasItemCtrl::set_size_extra(int extra)
{
    defer([ = ] {
        if (_extra == extra || _pixbuf) return; // Don't enlarge pixbuf!
        _width  += extra - _extra;
        _height += extra - _extra;
        _extra = extra;
        _built.reset();
        request_update(); // Geometry change
    });
}

void CanvasItemCtrl::set_type(CanvasItemCtrlType type)
{
    defer([ = ] {
        if (_handle._type == type) return;
        _handle._type = type;
        // Use _type to set default values.
        set_shape_default();
        set_size_default();
        _built.reset();
        request_update(); // Possible geometry change
    });
}

void CanvasItemCtrl::set_selected(bool selected)
{
    _handle.setSelected(selected);
    _built.reset();
    request_update();
}
void CanvasItemCtrl::set_click(bool click)
{
    _handle.setClick(click);
    _built.reset();
    request_update();
}
void CanvasItemCtrl::set_hover(bool hover)
{
    _handle.setHover(hover);
    _built.reset();
    request_update();
}
void CanvasItemCtrl::set_normal(bool selected)
{
    _handle.setSelected(selected);
    _handle.setHover(0);
    _handle.setClick(0);
    _built.reset();
    request_update();
}

void CanvasItemCtrl::set_angle(double angle)
{
    defer([ = ] {
        if (_angle == angle) return;
        _angle = angle;
        _built.reset();
        request_update(); // Geometry change
    });
}

void CanvasItemCtrl::set_anchor(SPAnchorType anchor)
{
    defer([ = ] {
        if (_anchor == anchor) return;
        _anchor = anchor;
        request_update(); // Geometry change
    });
}

// ---------- Protected ----------

static void draw_darrow(Cairo::RefPtr<Cairo::Context> const &cr, double size)
{
    // Find points, starting from tip of one arrowhead, working clockwise.
    /*   1        4
        ╱│        │╲
       ╱ └────────┘ ╲
     0╱  2        3  ╲5
      ╲  8        7  ╱
       ╲ ┌────────┐ ╱
        ╲│9      6│╱
    */

    // Length of arrowhead (not including stroke).
    double delta = (size - 1) / 4.0; // Use unscaled width.

    // Tip of arrow (0)
    double tip_x = 0.5;          // At edge, allow room for stroke.
    double tip_y = size / 2.0;   // Center, assuming width == height.

    // Outer corner (1)
    double out_x = tip_x + delta;
    double out_y = tip_y - delta;

    // Inner corner (2)
    double in_x = out_x;
    double in_y = out_y + (delta / 2.0);

    double x0 = tip_x;
    double y0 = tip_y;
    double x1 = out_x;
    double y1 = out_y;
    double x2 = in_x;
    double y2 = in_y;
    double x3 = size - in_x;
    double y3 = in_y;
    double x4 = size - out_x;
    double y4 = out_y;
    double x5 = size - tip_x;
    double y5 = tip_y;
    double x6 = size - out_x;
    double y6 = size - out_y;
    double x7 = size - in_x;
    double y7 = size - in_y;
    double x8 = in_x;
    double y8 = size - in_y;
    double x9 = out_x;
    double y9 = size - out_y;

    // Draw arrow
    cr->move_to(x0, y0);
    cr->line_to(x1, y1);
    cr->line_to(x2, y2);
    cr->line_to(x3, y3);
    cr->line_to(x4, y4);
    cr->line_to(x5, y5);
    cr->line_to(x6, y6);
    cr->line_to(x7, y7);
    cr->line_to(x8, y8);
    cr->line_to(x9, y9);
    cr->close_path();
}

static void draw_carrow(Cairo::RefPtr<Cairo::Context> const &cr, double size)
{
    // Length of arrowhead (not including stroke).
    double delta = (size - 3) / 4.0; // Use unscaled width.

    // Tip of arrow
    double tip_x =         1.5;  // Edge, allow room for stroke when rotated.
    double tip_y = delta + 1.5;

    // Outer corner (1)
    double out_x = tip_x + delta;
    double out_y = tip_y - delta;

    // Inner corner (2)
    double in_x = out_x;
    double in_y = out_y + (delta / 2.0);

    double x0 = tip_x;
    double y0 = tip_y;
    double x1 = out_x;
    double y1 = out_y;
    double x2 = in_x;
    double y2 = in_y;
    double x3 = size - in_y;        //double y3 = size - in_x;
    double x4 = size - out_y;
    double y4 = size - out_x;
    double x5 = size - tip_y;
    double y5 = size - tip_x;
    double x6 = x5 - delta;
    double y6 = y4;
    double x7 = x5 - delta / 2.0;
    double y7 = y4;
    double x8 = x1;                 //double y8 = y0 + delta/2.0;
    double x9 = x1;
    double y9 = y0 + delta;

    // Draw arrow
    cr->move_to(x0, y0);
    cr->line_to(x1, y1);
    cr->line_to(x2, y2);
    cr->arc(x1, y4, x3 - x2, 3.0 * M_PI / 2.0, 0);
    cr->line_to(x4, y4);
    cr->line_to(x5, y5);
    cr->line_to(x6, y6);
    cr->line_to(x7, y7);
    cr->arc_negative(x1, y4, x7 - x8, 0, 3.0 * M_PI / 2.0);
    cr->line_to(x9, y9);
    cr->close_path();
}

static void draw_triangle(Cairo::RefPtr<Cairo::Context> const &cr, double size)
{
    // Construct an arrowhead (triangle)
    double s = size / 2.0;
    double wcos = s * cos(M_PI / 6);
    double hsin = s * sin(M_PI / 6);
    // Construct a smaller arrow head for fill.
    Geom::Point p1f(1, s);
    Geom::Point p2f(s + wcos - 1, s + hsin);
    Geom::Point p3f(s + wcos - 1, s - hsin);
    // Draw arrow
    cr->move_to(p1f[0], p1f[1]);
    cr->line_to(p2f[0], p2f[1]);
    cr->line_to(p3f[0], p3f[1]);
    cr->close_path();
}

static void draw_triangle_angled(Cairo::RefPtr<Cairo::Context> const &cr, double size)
{
    // Construct an arrowhead (triangle) of half size.
    double s = size / 2.0;
    double wcos = s * cos(M_PI / 9);
    double hsin = s * sin(M_PI / 9);
    Geom::Point p1f(s + 1, s);
    Geom::Point p2f(s + wcos - 1, s + hsin - 1);
    Geom::Point p3f(s + wcos - 1, s - (hsin - 1));
    // Draw arrow
    cr->move_to(p1f[0], p1f[1]);
    cr->line_to(p2f[0], p2f[1]);
    cr->line_to(p3f[0], p3f[1]);
    cr->close_path();
}

static void draw_pivot(Cairo::RefPtr<Cairo::Context> const &cr, double size)
{
    double delta4 = (size - 5) / 4.0; // Keep away from edge or will clip when rotating.
    double delta8 = delta4 / 2;

    // Line start
    double center = size / 2.0;

    cr->move_to(center - delta8, center - 2 * delta4 - delta8);
    cr->rel_line_to(delta4,  0);
    cr->rel_line_to(0,       delta4);

    cr->rel_line_to(delta4,  delta4);

    cr->rel_line_to(delta4,  0);
    cr->rel_line_to(0,       delta4);
    cr->rel_line_to(-delta4,  0);

    cr->rel_line_to(-delta4,  delta4);

    cr->rel_line_to(0,       delta4);
    cr->rel_line_to(-delta4,  0);
    cr->rel_line_to(0,      -delta4);

    cr->rel_line_to(-delta4, -delta4);

    cr->rel_line_to(-delta4,  0);
    cr->rel_line_to(0,      -delta4);
    cr->rel_line_to(delta4,  0);

    cr->rel_line_to(delta4, -delta4);
    cr->close_path();

    cr->begin_new_sub_path();
    cr->arc_negative(center, center, delta4, 0, -2 * M_PI);
}

static void draw_salign(Cairo::RefPtr<Cairo::Context> const &cr, double size)
{
    // Triangle pointing at line.

    // Basic units.
    double delta4 = (size - 1) / 4.0; // Use unscaled width.
    double delta8 = delta4 / 2;
    if (delta8 < 2) {
        // Keep a minimum gap of at least one pixel (after stroking).
        delta8 = 2;
    }

    // Tip of triangle
    double tip_x = size / 2.0; // Center (also rotation point).
    double tip_y = size / 2.0;

    // Corner triangle position.
    double outer = size / 2.0 - delta4;

    // Outer line position
    double oline = size / 2.0 + (int)delta4;

    // Inner line position
    double iline = size / 2.0 + (int)delta8;

    // Draw triangle
    cr->move_to(tip_x,           tip_y);
    cr->line_to(outer,           outer);
    cr->line_to(size - outer,    outer);
    cr->close_path();

    // Draw line
    cr->move_to(outer,           iline);
    cr->line_to(size - outer,    iline);
    cr->line_to(size - outer,    oline);
    cr->line_to(outer,           oline);
    cr->close_path();
}

static void draw_calign(Cairo::RefPtr<Cairo::Context> const &cr, double size)
{
    // Basic units.
    double delta4 = (size - 1) / 4.0; // Use unscaled width.
    double delta8 = delta4 / 2;
    if (delta8 < 2) {
        // Keep a minimum gap of at least one pixel (after stroking).
        delta8 = 2;
    }

    // Tip of triangle
    double tip_x = size / 2.0; // Center (also rotation point).
    double tip_y = size / 2.0;

    // Corner triangle position.
    double outer = size / 2.0 - delta8 - delta4;

    // End of line positin
    double eline = size / 2.0 - delta8;

    // Outer line position
    double oline = size / 2.0 + (int)delta4;

    // Inner line position
    double iline = size / 2.0 + (int)delta8;

    // Draw triangle
    cr->move_to(tip_x,           tip_y);
    cr->line_to(outer,           tip_y);
    cr->line_to(tip_x,           outer);
    cr->close_path();

    // Draw line
    cr->move_to(iline,           iline);
    cr->line_to(iline,           eline);
    cr->line_to(oline,           eline);
    cr->line_to(oline,           oline);
    cr->line_to(eline,           oline);
    cr->line_to(eline,           iline);
    cr->close_path();
}

static void draw_malign(Cairo::RefPtr<Cairo::Context> const &cr, double size)
{
    // Basic units.
    double delta4 = (size - 1) / 4.0; // Use unscaled width.
    double delta8 = delta4 / 2;
    if (delta8 < 2) {
        // Keep a minimum gap of at least one pixel (after stroking).
        delta8 = 2;
    }

    // Tip of triangle
    double tip_0 = size / 2.0;
    double tip_1 = size / 2.0 - delta8;

    // Draw triangles
    cr->move_to(tip_0,           tip_1);
    cr->line_to(tip_0 - delta4,  tip_1 - delta4);
    cr->line_to(tip_0 + delta4,  tip_1 - delta4);
    cr->close_path();

    cr->move_to(size - tip_1,           tip_0);
    cr->line_to(size - tip_1 + delta4,  tip_0 - delta4);
    cr->line_to(size - tip_1 + delta4,  tip_0 + delta4);
    cr->close_path();

    cr->move_to(size - tip_0,           size - tip_1);
    cr->line_to(size - tip_0 + delta4,  size - tip_1 + delta4);
    cr->line_to(size - tip_0 - delta4,  size - tip_1 + delta4);
    cr->close_path();

    cr->move_to(tip_1,           tip_0);
    cr->line_to(tip_1 - delta4,  tip_0 + delta4);
    cr->line_to(tip_1 - delta4,  tip_0 - delta4);
    cr->close_path();
}

/**
 * Update and redraw control ctrl.
 */
void CanvasItemCtrl::_update(bool)
{
    // Queue redraw of old area (erase previous content).
    request_redraw();

    // Setting the position to (inf, inf) to hide it is a pervasive hack we need to support.
    if (!_position.isFinite()) {
        _bounds = {};
        return;
    }

    // Width and height are always odd.
    assert(_width % 2 == 1);
    assert(_height % 2 == 1);

    // Get half width and height, rounded down.
    int const w_half = _width / 2;
    int const h_half = _height / 2;

    // Set _angle, and compute adjustment for anchor.
    int dx = 0;
    int dy = 0;

    switch (_shape) {
    case CANVAS_ITEM_CTRL_SHAPE_DARROW:
    case CANVAS_ITEM_CTRL_SHAPE_SARROW:
    case CANVAS_ITEM_CTRL_SHAPE_CARROW:
    case CANVAS_ITEM_CTRL_SHAPE_SALIGN:
    case CANVAS_ITEM_CTRL_SHAPE_CALIGN: {
        double angle = _anchor * M_PI_4 + angle_of(affine());
        double const half = _width / 2.0;

        dx = -(half + 2) * cos(angle); // Add a bit to prevent tip from overlapping due to rounding errors.
        dy = -(half + 2) * sin(angle);

        switch (_shape) {
        case CANVAS_ITEM_CTRL_SHAPE_CARROW:
            angle += 5 * M_PI_4;
            break;

        case CANVAS_ITEM_CTRL_SHAPE_SARROW:
            angle += M_PI_2;
            break;

        case CANVAS_ITEM_CTRL_SHAPE_SALIGN:
            dx = -(half / 2 + 2) * cos(angle);
            dy = -(half / 2 + 2) * sin(angle);
            angle -= M_PI_2;
            break;

        case CANVAS_ITEM_CTRL_SHAPE_CALIGN:
            angle -= M_PI_4;
            dx = (half / 2 + 2) * (sin(angle) - cos(angle));
            dy = (half / 2 + 2) * (-sin(angle) - cos(angle));
            break;

        default:
            break;
        }

        if (_angle != angle) {
            _angle = angle;
            _built.reset();
        }

        break;
    }

    case CANVAS_ITEM_CTRL_SHAPE_PIVOT:
    case CANVAS_ITEM_CTRL_SHAPE_MALIGN: {
        double const angle = angle_of(affine());
        if (_angle != angle) {
            _angle = angle;
            _built.reset();
        }
        break;
    }

    default:
        switch (_anchor) {
        case SP_ANCHOR_N:
        case SP_ANCHOR_CENTER:
        case SP_ANCHOR_S:
            break;

        case SP_ANCHOR_NW:
        case SP_ANCHOR_W:
        case SP_ANCHOR_SW:
            dx = w_half;
            break;

        case SP_ANCHOR_NE:
        case SP_ANCHOR_E:
        case SP_ANCHOR_SE:
            dx = -w_half;
            break;
        }

        switch (_anchor) {
        case SP_ANCHOR_W:
        case SP_ANCHOR_CENTER:
        case SP_ANCHOR_E:
            break;

        case SP_ANCHOR_NW:
        case SP_ANCHOR_N:
        case SP_ANCHOR_NE:
            dy = h_half;
            break;

        case SP_ANCHOR_SW:
        case SP_ANCHOR_S:
        case SP_ANCHOR_SE:
            dy = -h_half;
            break;
        }
        break;
    }

    auto const pt = Geom::IntPoint(-w_half, -h_half) + Geom::IntPoint(dx, dy) + (_position * affine()).floor();
    _bounds = Geom::IntRect(pt, pt + Geom::IntPoint(_width, _height));

    // Queue redraw of new area
    request_redraw();
}

static inline uint32_t compose_xor(uint32_t bg, uint32_t fg, uint32_t a)
{
    uint32_t c = bg * (255 - a) + (((bg ^ ~fg) + (bg >> 2) - (bg > 127 ? 63 : 0)) & 255) * a;
    return (c + 127) / 255;
}

/**
 * Render ctrl to screen via Cairo.
 */
void CanvasItemCtrl::_render(CanvasItemBuffer &buf) const
{
    _parsed.init([ &, this] {
        parse_handle_styles();
    });

    _built.init([ &, this] {
        build_cache(buf.device_scale);
    });

    Geom::Point c = _bounds->min() - buf.rect.min();
    int x = c.x(); // Must be pixel aligned.
    int y = c.y();

    buf.cr->save();

    // This code works regardless of source type.

    // 1. Copy the affected part of output to a temporary surface

    // Size in device pixels. Does not set device scale.
    int width  = _width  * buf.device_scale;
    int height = _height * buf.device_scale;
    auto work = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, width, height);
    cairo_surface_set_device_scale(work->cobj(), buf.device_scale, buf.device_scale); // No C++ API!

    // std::cout<<width<<" "<<height<<std::endl;

    auto cr = Cairo::Context::create(work);
    cr->translate(-_bounds->left(), -_bounds->top());
    cr->set_source(buf.cr->get_target(), buf.rect.left(), buf.rect.top());
    cr->paint();
    // static int a = 0;
    // std::string name0 = "ctrl0_" + _name + "_" + std::to_string(a++) + ".png";
    // work->write_to_png(name0);

    // 2. Composite the control on a temporary surface
    work->flush();
    int strideb = work->get_stride();
    unsigned char *pxb = work->get_data();

    // this code allow background become isolated from rendering so we can do things like outline overlay
    uint32_t backcolor = get_canvas()->get_effective_background();
    uint32_t *p = _cache.get();
    for (int i = 0; i < height; ++i) {
        auto pb = reinterpret_cast<uint32_t *>(pxb + i * strideb);
        for (int j = 0; j < width; ++j) {
            uint32_t base = *pb;
            uint32_t cc = *p++;
            uint32_t ac = cc & 0xff;
            if (*pb == 0 && cc != 0) {
                base = backcolor;
            }
            if (ac == 0 && cc != 0) {
                *pb++ = argb32_from_rgba(cc | 0x000000ff);
            }
            else if (ac == 0) {
                *pb++ = base;
            }
            else if (
                _mode == CANVAS_ITEM_CTRL_MODE_XOR ||
                _mode == CANVAS_ITEM_CTRL_MODE_GRAYSCALED_XOR ||
                _mode == CANVAS_ITEM_CTRL_MODE_DESATURATED_XOR) {
                EXTRACT_ARGB32(base, ab, rb, gb, bb)
                // here we get canvas color and if color to draw
                // has opacity, we override base colors
                // flattenig canvas color
                EXTRACT_ARGB32(backcolor, abb, rbb, gbb, bbb)
                if (abb != ab) {
                    rb = (ab / 255.0) * rb + (1 - (ab / 255.0)) * rbb;
                    gb = (ab / 255.0) * gb + (1 - (ab / 255.0)) * gbb;
                    bb = (ab / 255.0) * bb + (1 - (ab / 255.0)) * bbb;
                    ab = 255;
                }
                uint32_t ro = compose_xor(rb, (cc & 0xff000000) >> 24, ac);
                uint32_t go = compose_xor(gb, (cc & 0x00ff0000) >> 16, ac);
                uint32_t bo = compose_xor(bb, (cc & 0x0000ff00) >>  8, ac);
                if (_mode == CANVAS_ITEM_CTRL_MODE_GRAYSCALED_XOR ||
                        _mode == CANVAS_ITEM_CTRL_MODE_DESATURATED_XOR) {
                    uint32_t gray = ro * 0.299 + go * 0.587 + bo * 0.114;
                    if (_mode == CANVAS_ITEM_CTRL_MODE_DESATURATED_XOR) {
                        double f = 0.85; // desaturate by 15%
                        double p = sqrt(ro * ro * 0.299 + go * go *  0.587 + bo * bo * 0.114);
                        ro = p + (ro - p) * f;
                        go = p + (go - p) * f;
                        bo = p + (bo - p) * f;
                    }
                    else {
                        ro = gray;
                        go = gray;
                        bo = gray;
                    }
                }
                ASSEMBLE_ARGB32(px, ab, ro, go, bo)
                *pb++ = px;
            }
            else {
                *pb++ = argb32_from_rgba(cc | 0x000000ff);
            }
        }
    }
    work->mark_dirty();
    // std::string name1 = "ctrl1_" + _name + "_" + std::to_string(a) + ".png";
    // work->write_to_png(name1);

    // 3. Replace the affected part of output with contents of temporary surface
    buf.cr->set_source(work, x, y);

    buf.cr->rectangle(x, y, _width, _height);
    buf.cr->clip();
    buf.cr->set_operator(Cairo::OPERATOR_SOURCE);
    buf.cr->paint();
    buf.cr->restore();
}

std::unordered_map<std::string, CanvasItemCtrlType> type_map = {
    {".inkscape-node-auto", CANVAS_ITEM_CTRL_TYPE_NODE_AUTO},
    {".inkscape-node-smooth", CANVAS_ITEM_CTRL_TYPE_NODE_SMOOTH},
    {".inkscape-node-cusp", CANVAS_ITEM_CTRL_TYPE_NODE_CUSP},
    {".inkscape-node-symmetrical", CANVAS_ITEM_CTRL_TYPE_NODE_SYMETRICAL},
    {".inkscape-anchor", CANVAS_ITEM_CTRL_TYPE_ANCHOR},
    {".inkscape-rotate", CANVAS_ITEM_CTRL_TYPE_ROTATE},
    {".inkscape-margin", CANVAS_ITEM_CTRL_TYPE_MARGIN},
    {".inkscape-center", CANVAS_ITEM_CTRL_TYPE_CENTER},
    {".inkscape-sizer", CANVAS_ITEM_CTRL_TYPE_SIZER},
    {".inkscape-shaper", CANVAS_ITEM_CTRL_TYPE_SHAPER},
    {".inkscape-lpe", CANVAS_ITEM_CTRL_TYPE_LPE},
    {".inkscape-point", CANVAS_ITEM_CTRL_TYPE_POINT},
    {".inkscape-adj-handle", CANVAS_ITEM_CTRL_TYPE_ADJ_HANDLE},
    {".inkscape-adj-skew", CANVAS_ITEM_CTRL_TYPE_ADJ_SKEW},
    {".inkscape-adj-rotate", CANVAS_ITEM_CTRL_TYPE_ADJ_ROTATE},
    {".inkscape-adj-center", CANVAS_ITEM_CTRL_TYPE_ADJ_CENTER},
    {".inkscape-adj-salign", CANVAS_ITEM_CTRL_TYPE_ADJ_SALIGN},
    {".inkscape-adj-calign", CANVAS_ITEM_CTRL_TYPE_ADJ_CALIGN},
    {".inkscape-adj-malign", CANVAS_ITEM_CTRL_TYPE_ADJ_MALIGN},
    {"*", CANVAS_ITEM_CTRL_TYPE_DEFAULT}
};

std::unordered_map<std::string, CanvasItemCtrlShape> shape_map = {
    {"\'square\'", CANVAS_ITEM_CTRL_SHAPE_SQUARE},
    {"\'diamond\'", CANVAS_ITEM_CTRL_SHAPE_DIAMOND},
    {"\'circle\'", CANVAS_ITEM_CTRL_SHAPE_CIRCLE},
    {"\'cross\'", CANVAS_ITEM_CTRL_SHAPE_CROSS},
    {"\'plus\'", CANVAS_ITEM_CTRL_SHAPE_PLUS},
    {"\'pivot\'", CANVAS_ITEM_CTRL_SHAPE_PIVOT},
    {"\'arrow\'", CANVAS_ITEM_CTRL_SHAPE_DARROW},
    {"\'skew-arrow\'", CANVAS_ITEM_CTRL_SHAPE_SARROW},
    {"\'curved-arrow\'", CANVAS_ITEM_CTRL_SHAPE_CARROW},
    {"\'side-align\'", CANVAS_ITEM_CTRL_SHAPE_SALIGN},
    {"\'corner-align\'", CANVAS_ITEM_CTRL_SHAPE_CALIGN},
    {"\'middle-align\'", CANVAS_ITEM_CTRL_SHAPE_MALIGN}
};

std::vector<std::pair<HandleStyle *, int>> selected_handles;

void configure_selector(CRSelector *a_selector, Handle *&selector, int &specificity)
{
    cr_simple_sel_compute_specificity(a_selector->simple_sel);
    specificity =  a_selector->simple_sel->specificity;
    const char *selector_str = reinterpret_cast<const char *>(cr_simple_sel_one_to_string(a_selector->simple_sel));
    char **tokens = g_strsplit(selector_str, ":", 0);
    CanvasItemCtrlType type;
    if (type_map.find(*tokens) != type_map.end()) {
        type = type_map[*tokens];
        tokens++;
    }
    else {
        std::cerr << "Unrecognized selector:" << selector_str << std::endl;
        selector = NULL;
        return;
    }
    selector = new Handle(type);
    for (; *tokens; tokens++) {
        if (*tokens == "*") {
            continue;
        }
        else if (!strcmp(*tokens, "selected")) {
            selector->setSelected(1);
        }
        else if (!strcmp(*tokens, "hover")) {
            specificity++;
            selector->setHover(1);
        }
        else if (!strcmp(*tokens, "click")) {
            specificity++;
            selector->setClick(1);
        }
        else {
            std::cerr << "Unrecognized selector:" << selector_str << std::endl;
            selector = NULL;
            return;
        }
    }
}

void set_selectors(CRDocHandler *a_handler, CRSelector *a_selector, bool is_users)
{
    while (a_selector) {
        Handle *selector;
        int specificity;
        configure_selector(a_selector, selector, specificity);
        if (selector) {
            for (const auto& [handle, style] : CanvasItemCtrl::handle_styles) {
                if (Handle::fits(*selector, handle)) {
                    selected_handles.push_back({style, specificity + 10000 * is_users});
                }
            }
        }
        a_selector = a_selector->next;
        delete selector;
    }
}

void set_selectors_user(CRDocHandler *a_handler, CRSelector *a_selector)
{
    set_selectors(a_handler, a_selector, true);
}

void set_selectors_base(CRDocHandler *a_handler, CRSelector *a_selector)
{
    set_selectors(a_handler, a_selector, false);
}

void set_properties(CRDocHandler *a_handler, CRString *a_name, CRTerm *a_value, gboolean a_important)
{
    //Refactor to use enum and a common function interface.

    const std::string value = (char *)cr_term_to_string(a_value);
    const std::string property = cr_string_peek_raw_str(a_name);
    if (property == "shape") {
        if (shape_map.find(value) != shape_map.end()) {
            for (auto& [handle, specificity] : selected_handles) {
                handle->shape.setProperty(shape_map[value], specificity + 100000 * a_important);
            }
        }
        else {
            std::cerr << "Unrecognized value for " << property << ": " << value << std::endl;
            return;
        }
    }
    else if (property == "fill" || property == "stroke") {
        CRRgb *rgb = cr_rgb_new();
        CRStatus status = cr_rgb_set_from_term(rgb, a_value);

        if (status == CR_OK) {
            ASSEMBLE_ARGB32(color, 255, (uint8_t)rgb->red, (uint8_t)rgb->green, (uint8_t)rgb->blue)
            for (auto& [handle, specificity] : selected_handles) {
                if (property == "fill") {
                    handle->fill.setProperty(color, specificity + 100000 * a_important);
                }
                else {
                    handle->stroke.setProperty(color, specificity + 100000 * a_important);
                }
            }
        }
        else {
            std::cerr << "Unrecognized value for " << property << ": " << value << std::endl;
            return;
        }
    }
    else if (property == "opacity" || property == "fill-opacity" || property == "stroke-opacity") {
        if (!a_value->content.num) {
            std::cerr << "Invalid value for " << property << ": " << value << std::endl;
            return;
        }

        double val;
        if (a_value->content.num->type == NUM_PERCENTAGE) {
            val = (a_value->content.num->val) / 100.0f;
        }
        else if (a_value->content.num->type == NUM_GENERIC) {
            val = a_value->content.num->val;
        }
        else {
            std::cerr << "Invalid type for " << property << ": " << value << std::endl;
            return;
        }

        if (val > 1 || val < 0) {
            std::cerr << "Invalid value for " << property << ": " << value << std::endl;
            return;
        }

        for (auto& [handle, specificity] : selected_handles) {
            if (property == "opacity") {
                handle->opacity.setProperty(val, specificity + 100000 * a_important);
            }
            else if (property == "fill-opacity") {
                handle->fill_opacity.setProperty(val, specificity + 100000 * a_important);
            }
            else if (property == "stroke-opacity") {
                handle->stroke_opacity.setProperty(val, specificity + 100000 * a_important);
            }
        }
    }
    else if (property == "stroke-width") {
        //Assuming px value only, which stays the same regardless of the size of the handles.
        int val;
        if (!a_value->content.num) {
            std::cerr << "Invalid value for " << property << ": " << value << std::endl;
            return;
        }
        if (a_value->content.num->type == NUM_LENGTH_PX) {
            val = int(a_value->content.num->val);
        }
        else {
            std::cerr << "Invalid type for " << property << ": " << value << std::endl;
            return;
        }

        for (auto& [handle, specificity] : selected_handles) {
            handle->stroke_width.setProperty(val, specificity + 100000 * a_important);
        }
    }
    else {
        std::cerr << "Unrecognized property:" << property << std::endl;
    }
}

void clear_selectors(CRDocHandler *a_handler, CRSelector *a_selector)
{
    selected_handles.clear();
}

void CanvasItemCtrl::parse_handle_styles() const
{
    for (int type_i = CANVAS_ITEM_CTRL_TYPE_DEFAULT; type_i <= CANVAS_ITEM_CTRL_TYPE_NODE_SYMETRICAL; type_i++) {
        auto type = static_cast<CanvasItemCtrlType>(type_i);
        for (auto state = 0; state <= 8; state++) {
            delete handle_styles[Handle(type,state)];
            handle_styles[Handle(type,state)] = new HandleStyle();
        }
    }

    CRDocHandler *sac = cr_doc_handler_new();
    sac->start_selector = set_selectors_base;
    sac->property = set_properties;
    sac->end_selector = clear_selectors;

    auto base_css_path = Inkscape::IO::Resource::get_path_ustring(Inkscape::IO::Resource::SYSTEM, Inkscape::IO::Resource::UIS, "node-handles.css");
    if (Inkscape::IO::file_test(base_css_path.c_str(), G_FILE_TEST_EXISTS)) {
        CRParser *base_parser = cr_parser_new_from_file(reinterpret_cast<const guchar *>(base_css_path.c_str()), CR_ASCII);
        cr_parser_set_sac_handler(base_parser, sac);
        cr_parser_parse(base_parser);
    }

    auto user_css_path = Inkscape::IO::Resource::get_path_ustring(Inkscape::IO::Resource::USER, Inkscape::IO::Resource::UIS, "node-handles.css");
    if (Inkscape::IO::file_test(user_css_path.c_str(), G_FILE_TEST_EXISTS)) {
        CRParser *user_parser = cr_parser_new_from_file(reinterpret_cast<const guchar *>(user_css_path.c_str()), CR_ASCII);
        sac->start_selector = set_selectors_user;
        cr_parser_set_sac_handler(user_parser, sac);
        cr_parser_parse(user_parser);
    }
}

void CanvasItemCtrl::build_cache(int device_scale) const
{
    if (_width < 2 || _height < 2) {
        return; // Nothing to render
    }

    if (_shape != CANVAS_ITEM_CTRL_SHAPE_BITMAP) {
        if (_width % 2 == 0 || _height % 2 == 0) {
            std::cerr << "CanvasItemCtrl::build_cache: Width and/or height not odd integer! "
                      << _name << ":  width: " << _width << "  height: " << _height << std::endl;
        }
    }

    // Get memory for cache.
    int width  = _width  * device_scale;  // Not unsigned or math errors occur!
    int height = _height * device_scale;
    int size = width * height;

    _cache = std::make_unique<uint32_t[]>(size);
    if (handle_cache.find(_handle) != handle_cache.end() &&
            handle_cache[_handle].find(size) != handle_cache[_handle].end()) {
        _cache = handle_cache[_handle][size];
    }
    else if (handle_styles.find(_handle) != handle_styles.end()) {
        auto handle = handle_styles[_handle];
        auto shape = handle->shape();
        auto fill = handle->getFill();
        auto stroke = handle->getStroke();
        auto stroke_width = handle->stroke_width();
        build_shape(_cache, shape, fill, stroke, stroke_width, height, width, _angle, _pixbuf, device_scale);
        handle_cache[_handle][size] = _cache;
    }
    else {
        // std::cout << _handle._type << std::endl;
        build_shape(_cache, _shape, _fill, _stroke, 1, height, width, _angle, _pixbuf, device_scale);
        handle_cache[_handle][size] = _cache;
    }
}

void CanvasItemCtrl::build_shape(std::shared_ptr<uint32_t[]> cache,
                                 CanvasItemCtrlShape shape, uint32_t fill, uint32_t stroke,
                                 int stroke_width, int height, int width, double angle,
                                 Glib::RefPtr<Gdk::Pixbuf> pixbuf, int device_scale)
{
    int scaled_width = device_scale * stroke_width;
    auto p = cache.get();
    switch (shape) {
    case CANVAS_ITEM_CTRL_SHAPE_SQUARE:
        // Actually any rectanglular shape.
        for (int i = 0; i < width; ++i) {
            for (int j = 0; j < width; ++j) {
                if (i + 1 > scaled_width && scaled_width < width  - i &&
                        j + 1 > scaled_width && scaled_width < height - j) {
                    *p++ = fill;
                }
                else {
                    *p++ = stroke;
                }
            }
        }
        break;

    case CANVAS_ITEM_CTRL_SHAPE_DIAMOND: {
        // Assume width == height.
        int m = (width + 1) / 2;

        for (int i = 0; i < width; ++i) {
            for (int j = 0; j < height; ++j) {
                if (i  +           j  > m - 1 + scaled_width &&
                        (width - 1 - i) +           j  > m - 1 + scaled_width &&
                        (width - 1 - i) + (height - 1 - j) > m - 1 + scaled_width &&
                        i    + (height - 1 - j) > m - 1 + scaled_width) {
                    *p++ = fill;
                }
                else if (i  +           j  > m - 2 &&
                         (width - 1 - i) +           j  > m - 2 &&
                         (width - 1 - i) + (height - 1 - j) > m - 2 &&
                         i    + (height - 1 - j) > m - 2) {
                    *p++ = stroke;
                }
                else {
                    *p++ = 0;
                }
            }
        }
        break;
    }

    case CANVAS_ITEM_CTRL_SHAPE_CIRCLE: {
        // Assume width == height.
        double rs  = width / 2.0;
        double rs2 = rs * rs;
        double rf  = rs - scaled_width;
        double rf2 = rf * rf;

        for (int i = 0; i < width; ++i) {
            for (int j = 0; j < height; ++j) {

                double rx = i - (width / 2.0) + 0.5;
                double ry = j - (height / 2.0) + 0.5;
                double r2 = rx * rx + ry * ry;

                if (r2 < rf2) {
                    *p++ = fill;
                }
                else if (r2 < rs2) {
                    *p++ = stroke;
                }
                else {
                    *p++ = 0;
                }
            }
        }
        break;
    }

    case CANVAS_ITEM_CTRL_SHAPE_CROSS: {
        // Actually an 'X'.
        double sw2 = scaled_width * sqrt(2);
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                if ((abs(x - y) < sw2 && abs(x + y - width) < width - sw2) ||
                        (abs(x + y - width) < sw2 && abs(x - y) < width - sw2)) {
                    *p++ = stroke;
                }
                else {
                    *p++ = 0;
                }
            }
        }
        break;
    }

    case CANVAS_ITEM_CTRL_SHAPE_PLUS:
        // Actually an '+'.
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                if (std::abs(x - width / 2)   < scaled_width ||
                        std::abs(y - height / 2)  < scaled_width) {
                    *p++ = stroke;
                }
                else {
                    *p++ = 0;
                }
            }
        }
        break;

    case CANVAS_ITEM_CTRL_SHAPE_TRIANGLE:        //triangle optionaly rotated
    case CANVAS_ITEM_CTRL_SHAPE_TRIANGLE_ANGLED: // triangle with pointing to center of  knot and rotated this way
    case CANVAS_ITEM_CTRL_SHAPE_DARROW:          // Double arrow
    case CANVAS_ITEM_CTRL_SHAPE_SARROW:          // Same shape as darrow but rendered rotated 90 degrees.
    case CANVAS_ITEM_CTRL_SHAPE_CARROW:          // Double corner arrow
    case CANVAS_ITEM_CTRL_SHAPE_PIVOT:           // Fancy "plus"
    case CANVAS_ITEM_CTRL_SHAPE_SALIGN:          // Side align (triangle pointing toward line)
    case CANVAS_ITEM_CTRL_SHAPE_CALIGN:          // Corner align (triangle pointing into "L")
    case CANVAS_ITEM_CTRL_SHAPE_MALIGN: {        // Middle align (four triangles poining inward)
        double size = width / device_scale; // Use unscaled width.

        auto work = Cairo::ImageSurface::create(Cairo::FORMAT_ARGB32, device_scale * size, device_scale * size);
        cairo_surface_set_device_scale(work->cobj(), device_scale, device_scale); // No C++ API!
        auto cr = Cairo::Context::create(work);

        // Rotate around center
        cr->translate(size / 2.0,  size / 2.0);
        cr->rotate(angle);
        cr->translate(-size / 2.0, -size / 2.0);

        // Construct path
        bool triangles = shape == CANVAS_ITEM_CTRL_SHAPE_TRIANGLE_ANGLED || shape == CANVAS_ITEM_CTRL_SHAPE_TRIANGLE;
        switch (shape) {
        case CANVAS_ITEM_CTRL_SHAPE_DARROW:
        case CANVAS_ITEM_CTRL_SHAPE_SARROW:
            draw_darrow(cr, size);
            break;

        case CANVAS_ITEM_CTRL_SHAPE_TRIANGLE:
            draw_triangle(cr, size);
            break;

        case CANVAS_ITEM_CTRL_SHAPE_TRIANGLE_ANGLED:
            draw_triangle_angled(cr, size);
            break;

        case CANVAS_ITEM_CTRL_SHAPE_CARROW:
            draw_carrow(cr, size);
            break;

        case CANVAS_ITEM_CTRL_SHAPE_PIVOT:
            draw_pivot(cr, size);
            break;

        case CANVAS_ITEM_CTRL_SHAPE_SALIGN:
            draw_salign(cr, size);
            break;

        case CANVAS_ITEM_CTRL_SHAPE_CALIGN:
            draw_calign(cr, size);
            break;

        case CANVAS_ITEM_CTRL_SHAPE_MALIGN:
            draw_malign(cr, size);
            break;

        default:
            // Shouldn't happen
            break;
        }

        // Fill and stroke.
        cr->set_source_rgba(SP_RGBA32_R_F(fill),
                            SP_RGBA32_G_F(fill),
                            SP_RGBA32_B_F(fill),
                            SP_RGBA32_A_F(fill));
        cr->fill_preserve();
        cr->set_source_rgba(SP_RGBA32_R_F(stroke),
                            SP_RGBA32_G_F(stroke),
                            SP_RGBA32_B_F(stroke),
                            SP_RGBA32_A_F(stroke));
        cr->set_line_width(stroke_width);
        cr->stroke();

        // Copy to buffer.
        work->flush();
        int strideb = work->get_stride();
        unsigned char *pxb = work->get_data();
        auto p = cache.get();
        for (int i = 0; i < device_scale * size; ++i) {
            auto pb = reinterpret_cast<uint32_t *>(pxb + i * strideb);
            for (int j = 0; j < width; ++j) {

                if (triangles) {
                    *p++ = rgba_from_argb32(*pb);
                }
                else {
                    uint32_t color = 0x0;

                    // Need to un-premultiply alpha and change order argb -> rgba.
                    uint32_t alpha = (*pb & 0xff000000) >> 24;
                    if (alpha == 0x0) {
                        color = 0x0;
                    }
                    else {
                        uint32_t rgb = unpremul_alpha(*pb & 0xffffff, alpha);
                        color = (rgb << 8) + alpha;
                    }
                    *p++ = color;
                }
                pb++;
            }
        }
        break;
    }

    case CANVAS_ITEM_CTRL_SHAPE_BITMAP: {
        if (pixbuf) {
            unsigned char *px = pixbuf->get_pixels();
            unsigned int   rs = pixbuf->get_rowstride();
            for (int y = 0; y < height / device_scale; y++) {
                for (int x = 0; x < width / device_scale; x++) {
                    unsigned char *s = px + rs * y + 4 * x;
                    uint32_t color;
                    if (s[3] < 0x80) {
                        color = 0;
                    }
                    else if (s[0] < 0x80) {
                        color = stroke;
                    }
                    else {
                        color = fill;
                    }

                    // Fill in device_scale x device_scale block
                    for (int i = 0; i < device_scale; ++i) {
                        for (int j = 0; j < device_scale; ++j) {
                            auto p = cache.get() +
                                     (x * device_scale + i) +            // Column
                                     (y * device_scale + j) * width;     // Row
                            *p = color;
                        }
                    }
                }
            }
        }
        else {
            std::cerr << "CanvasItemCtrl::build_cache: No bitmap!" << std::endl;
            auto p = cache.get();
            for (int y = 0; y < height / device_scale; y++) {
                for (int x = 0; x < width / device_scale; x++) {
                    if (x == y) {
                        *p++ = 0xffff0000;
                    }
                    else {
                        *p++ = 0;
                    }
                }
            }
        }
        break;
    }

    case CANVAS_ITEM_CTRL_SHAPE_IMAGE:
        std::cerr << "CanvasItemCtrl::build_cache: image: UNIMPLEMENTED" << std::endl;
        break;

    default:
        std::cerr << "CanvasItemCtrl::build_cache: unhandled shape!" << std::endl;
        break;
    }
}


} // namespace Inkscape

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
