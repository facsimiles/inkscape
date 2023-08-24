// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_CANVAS_ITEM_CTRL_H
#define SEEN_CANVAS_ITEM_CTRL_H

/**
 * A class to represent a control node.
 */

/*
 * Authors:
 *   Tavmjong Bah
 *   Sanidhya Singh
 *
 * Copyright (C) 2020 Tavmjong Bah
 *
 * Rewrite of SPCtrl
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <mutex>
#include <memory>
#include <2geom/point.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <boost/functional/hash.hpp>

#include "canvas-item.h"
#include "canvas-item-enums.h"

#include "enums.h" // SP_ANCHOR_X
#include "display/initlock.h"
#include "display/cairo-utils.h" // argb32_from_rgba()

namespace Inkscape {

/**
 * Struct to manage state and type.
 */
struct Handle {
    CanvasItemCtrlType _type;
    // 0b00....0<click-bit><hover-bit><selected-bit>
    uint32_t _state = 0;

    Handle() : _type(CANVAS_ITEM_CTRL_TYPE_DEFAULT), _state(0) {}
    Handle(CanvasItemCtrlType type) : _type(type), _state(0) {}
    Handle(CanvasItemCtrlType type, uint32_t state) : _type(type), _state(state) {}
    Handle(CanvasItemCtrlType type, bool selected, bool hover, bool click) : _type(type)
    {
        setState(selected, hover, click);
    }

    void setType(CanvasItemCtrlType type)
    {
        _type = type;
    }
    void setState(bool selected, bool hover, bool click)
    {
        setSelected(selected);
        setHover(hover);
        setClick(click);
    }
    void setState(uint32_t state)
    {
        _state |= state;
    }
    void setSelected(bool selected = 1)
    {
        _state &= ~(1);
        _state |= (selected);
    }
    bool isSelected() const
    {
        return _state & 1;
    }
    void setHover(bool hover = 1)
    {
        _state &= ~(1 << 1);
        _state |= (hover << 1);
    }
    bool isHover() const
    {
        return _state & (1 << 1);
    }
    void setClick(bool click = 1)
    {
        _state &= ~(1 << 2);
        _state |= (click << 2);
    }
    bool isClick() const
    {
        return _state & (1 << 2);
    }
    bool operator==(const Handle &other) const
    {
        return (_type == other._type && _state == other._state);
    }
    bool operator!=(const Handle &other) const
    {
        return !(*this == other);
    }
    static bool fits(const Handle &selector, const Handle &handle)
    {
        if (selector._type == CANVAS_ITEM_CTRL_TYPE_DEFAULT) {
            return ((selector._state & handle._state) == selector._state);
        }
        return (selector._type == handle._type) && ((selector._state & handle._state) == selector._state);
    }
};

/**
 * Template struct for properties with specificities.
 */
template <typename T>
struct Property {
    T value;
    uint32_t specificity = 0;

    /*
     * Set value of property based on specificity.
     */
    void setProperty(T newValue, uint32_t newSpecificity)
    {
        if (newSpecificity >= specificity) {
            value = newValue;
            specificity = newSpecificity;
        }
    }

    /*
     * Equivalent to calling .value
     */
    T &operator()()
    {
        return value;
    }
};

/**
 * Struct containing all required styling for handles.
 */
struct HandleStyle {
    Property<CanvasItemCtrlShape> shape{CANVAS_ITEM_CTRL_SHAPE_SQUARE};
    Property<uint32_t> fill{0xffffffff};
    Property<uint32_t> stroke{0xffffffff};
    Property<float> fill_opacity{1.0};
    Property<float> stroke_opacity{1.0};
    Property<float> opacity{1.0};
    Property<int> stroke_width{1};

    uint32_t getFill()
    {
        EXTRACT_ARGB32((fill()), a, r, g, b)
        a = int(fill_opacity() * opacity() * 255);
        ASSEMBLE_ARGB32(fill_color, a, r, g, b)
        return rgba_from_argb32(fill_color);
    }

    uint32_t getStroke()
    {
        EXTRACT_ARGB32(stroke(), stroke_a, stroke_r, stroke_g, stroke_b)
        EXTRACT_ARGB32(fill(), fill_a, fill_r, fill_g, fill_b)
        float fill_af = fill_opacity();
        float stroke_af = stroke_opacity();
        float result_af = stroke_af + fill_af * (1 - stroke_af);
        if (result_af == 0) {
            return 0;
        }

        uint8_t result_r = int((stroke_r * stroke_af + fill_r * fill_af * (1 - stroke_af)) / result_af);
        uint8_t result_g = int((stroke_g * stroke_af + fill_g * fill_af * (1 - stroke_af)) / result_af);
        uint8_t result_b = int((stroke_b * stroke_af + fill_b * fill_af * (1 - stroke_af)) / result_af);
        ASSEMBLE_ARGB32(blend, (int(opacity()*result_af * 255)), result_r, result_g, result_b)
        return rgba_from_argb32(blend);
    }
};

class CanvasItemCtrl : public CanvasItem {
public:
    CanvasItemCtrl(CanvasItemGroup *group);
    CanvasItemCtrl(CanvasItemGroup *group, CanvasItemCtrlType type);
    CanvasItemCtrl(CanvasItemGroup *group, CanvasItemCtrlType type, Geom::Point const &p);
    CanvasItemCtrl(CanvasItemGroup *group, CanvasItemCtrlShape shape);
    CanvasItemCtrl(CanvasItemGroup *group, CanvasItemCtrlShape shape, Geom::Point const &p);

    // Geometry
    void set_position(Geom::Point const &position);

    double closest_distance_to(Geom::Point const &p) const;

    // Selection
    bool contains(Geom::Point const &p, double tolerance = 0) override;

    // Properties
    void set_fill(uint32_t rgba) override;
    void set_stroke(uint32_t rgba) override;
    void set_shape(CanvasItemCtrlShape shape);
    // void set_shape_default(); // Use type to determine shape.
    void set_mode(CanvasItemCtrlMode mode);
    void set_mode_default();
    void set_size(int size);
    virtual void set_size_via_index(int size_index);
    void set_size_default(); // Use preference and type to set size.
    void set_size_extra(int extra); // Used to temporary increase size of ctrl.
    void set_anchor(SPAnchorType anchor);
    void set_angle(double angle);
    void set_type(CanvasItemCtrlType type);
    void set_pixbuf(Glib::RefPtr<Gdk::Pixbuf> pixbuf);
    void set_selected(bool selected = 1);
    void set_click(bool click = 1);
    void set_hover(bool hover = 1);
    void set_normal(bool selected = 0);
    static void build_shape(std::shared_ptr<uint32_t[]> cache,
                            CanvasItemCtrlShape shape, uint32_t fill, uint32_t stroke, int stroke_width,
                            int height, int width, double angle, Glib::RefPtr<Gdk::Pixbuf> pixbuf,
                            int device_scale);//TODO: add more style properties

protected:
    ~CanvasItemCtrl() override = default;

    void _update(bool propagate) override;
    void _render(Inkscape::CanvasItemBuffer &buf) const override;

    void build_cache(int device_scale) const;
    void parse_handle_styles() const;
    // Geometry
    Geom::Point _position;

    // Display
    InitLock _built;
    mutable std::shared_ptr<uint32_t[]> _cache;

    // Properties
    Handle _handle = Handle();
    CanvasItemCtrlShape _shape = CANVAS_ITEM_CTRL_SHAPE_SQUARE;
    CanvasItemCtrlMode  _mode  = CANVAS_ITEM_CTRL_MODE_XOR;
    int _width  = 5; // Nominally width == height == size... unless we use a pixmap.
    int _height = 5;
    int _extra  = 0; // Used to temporarily increase size.
    double _angle = 0; // Used for triangles, could be used for arrows.
    SPAnchorType _anchor = SP_ANCHOR_CENTER;
    Glib::RefPtr<Gdk::Pixbuf> _pixbuf;
};

} // namespace Inkscape

#endif // SEEN_CANVAS_ITEM_CTRL_H

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
