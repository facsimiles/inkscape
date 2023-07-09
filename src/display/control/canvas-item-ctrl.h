// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SEEN_CANVAS_ITEM_CTRL_H
#define SEEN_CANVAS_ITEM_CTRL_H

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

#include <memory>
#include <2geom/point.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

#include "canvas-item.h"
#include "canvas-item-enums.h"

#include "enums.h" // SP_ANCHOR_X
#include "display/initlock.h"

namespace Inkscape {

struct Handle {
    CanvasItemCtrlType _type;
    uint32_t _state;
    // 0b00....0<click-bit><hover-bit><selected-bit>

    Handle() : _type(CANVAS_ITEM_CTRL_TYPE_DEFAULT), _state(0) {}

    Handle(CanvasItemCtrlType type) : _type(type), _state(0) {}

    Handle(CanvasItemCtrlType type, uint32_t state) : _type(type), _state(state) {}

    //TODO: this function already exists in the code so adjust to use that one.
    void setType(CanvasItemCtrlType set_type)
    {
        _type = set_type;
    }
    void setState(uint32_t state)
    {
        _state |= state;
    }
    void setSelected(bool selected)
    {
        _state &= ~(1);
        _state |= (selected);
    }
    bool isSelected()
    {
        return _state & 1;
    }
    void setClick(bool clicked)
    {
        _state &= ~(1 << 1);
        _state |= (clicked << 1);
    }
    bool isClick()
    {
        return _state & 1 << 1;
    }
    void setHover(bool hover)
    {
        _state &= ~(1 << 2);
        _state |= (hover << 2);
    }
    bool isHover()
    {
        return _state & 1 << 1;
    }
    bool operator==(const Handle &other) const
    {
        return _type == other._type && _state == other._state;
    }
    bool operator!=(const Handle &other) const
    {
        return !(*this == other);
    }
    static bool fits(const Handle &selector, const Handle &handle)
    {
        std::cout << selector._type << " " << handle._type << std::endl;
        std::cout << selector._state << " " << handle._state << std::endl;
        if (selector._type == CANVAS_ITEM_CTRL_TYPE_DEFAULT) {
            return ((selector._state & handle._state) == selector._state);
        }
        return (selector._type == handle._type) && ((selector._state & handle._state) == selector._state);
    }
};

template <typename T>
struct Property {
    T value;
    uint32_t specificity = 0;

    void setProperty(T newValue, uint32_t newSpecificity)
    {
        if (newSpecificity >= specificity) {
            value = newValue;
            specificity = newSpecificity;
        }
    }

    // this is in alternate needing to call ->value everytime, now () can be called directly on the object.
    T operator()() const
    {
        return value;
    }
};

struct HandleStyle {
    Property<CanvasItemCtrlShape> shape;

    HandleStyle()
    {
        shape.value = CANVAS_ITEM_CTRL_SHAPE_SQUARE;
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
    void set_shape_default(); // Use type to determine shape.
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
    static std::unordered_map<Handle, HandleStyle *> handle_styles;

protected:
    ~CanvasItemCtrl() override = default;

    void _update(bool propagate) override;
    void _render(Inkscape::CanvasItemBuffer &buf) const override;

    void build_cache(int device_scale) const;
    void parse_and_build_cache() const;

    // Geometry
    Geom::Point _position;

    // Display
    InitLock _built;
    mutable std::unique_ptr<uint32_t[]> _cache;
    static InitLock _parsed;
    //Handle mapped to the Cache
    //mutable(might not need to make it mutable) static std::unordered_map<Handle,std::unique_ptr<uint32_t[]>> cache;

    // static mutable int does_it;

    // Properties
    //Handle _handle = Handle();
    CanvasItemCtrlType  _type  = CANVAS_ITEM_CTRL_TYPE_DEFAULT;
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

namespace std {
template <>
struct hash<Inkscape::Handle> {
    size_t operator()(Inkscape::Handle const &handle) const
    {
        uint64_t typeandstate = uint64_t(handle._type) << 32 | handle._state;
        return hash<uint64_t>()(typeandstate);
    }
};
} // namespace std

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
