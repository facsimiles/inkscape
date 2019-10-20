// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Group belonging to an SVG drawing element.
 *//*
 * Authors:
 *   Krzysztof Kosiński <tweenk.pl@gmail.com>
 *
 * Copyright (C) 2011 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "display/drawing-group.h"
#include "display/cairo-utils.h"
#include "display/drawing-context.h"
#include "display/drawing-item.h"
#include "display/drawing-surface.h"
#include "display/drawing.h"
#include "style.h"

namespace Inkscape {

DrawingGroup::DrawingGroup(Drawing &drawing)
    : DrawingItem(drawing)
    , _child_transform(nullptr)
{}

DrawingGroup::~DrawingGroup()
{
    delete _child_transform; // delete NULL; is safe
}

/**
 * Set whether the group returns children from pick calls.
 * Previously this feature was called "transparent groups".
 */
void
DrawingGroup::setPickChildren(bool p)
{
    _pick_children = p;
}

/**
 * Set additional transform for the group.
 * This is applied after the normal transform and mainly useful for
 * markers, clipping paths, etc.
 */
void
DrawingGroup::setChildTransform(Geom::Affine const &new_trans)
{
    Geom::Affine current;
    if (_child_transform) {
        current = *_child_transform;
    }

    if (!Geom::are_near(current, new_trans, 1e-18)) {
        // mark the area where the object was for redraw.
        _markForRendering();
        if (new_trans.isIdentity()) {
            delete _child_transform; // delete NULL; is safe
            _child_transform = nullptr;
        } else {
            _child_transform = new Geom::Affine(new_trans);
        }
        _markForUpdate(STATE_ALL, true);
    }
}

unsigned
DrawingGroup::_updateItem(Geom::IntRect const &area, UpdateContext const &ctx, unsigned flags, unsigned reset)
{
    unsigned beststate = STATE_ALL;
    bool outline = _drawing.outline();

    UpdateContext child_ctx(ctx);
    if (_child_transform) {
        child_ctx.ctm = *_child_transform * ctx.ctm;
    }
    for (auto & i : _children) {
        i.update(area, child_ctx, flags, reset);
    }
    if (beststate & STATE_BBOX) {
        _bbox = Geom::OptIntRect();
        for (auto & i : _children) {
            if (i.visible()) {
                _bbox.unionWith(outline ? i.geometricBounds() : i.visualBounds());
            }
        }
    }
    return beststate;
}

unsigned
DrawingGroup::_renderItem(DrawingContext &dc, Geom::IntRect const &area, unsigned flags, DrawingItem *stop_at)
{
    bool isolated = false;
    if (!parent() || _isolation == SP_CSS_ISOLATION_ISOLATE || _mix_blend_mode) {
        isolated = true;
    }
    int device_scale = dc.surface()->device_scale();
    DrawingSurface intermediate(area, device_scale);
    DrawingContext ict(intermediate);
    ict.setOperator(CAIRO_OPERATOR_OVER);

    if (stop_at == nullptr) {
        // normal rendering
        for (auto &i : _children) {
            i.setAntialiasing(_antialias);
            i.render(isolated ? ict : dc, area, flags, stop_at);
        }
    } else {
        // background rendering
        for (auto &i : _children) {
            if (&i == stop_at)
                return RENDER_OK; // do not render the stop_at item at all
            if (i.isAncestorOf(stop_at)) {
                // render its ancestors without masks, opacity or filters
                i.setAntialiasing(_antialias);
                i.render(isolated ? ict : dc, area, flags | RENDER_FILTER_BACKGROUND, stop_at);
                break;
            } else {
                i.setAntialiasing(_antialias);
                i.render(isolated ? ict : dc, area, flags, stop_at);
            }
        }
    }
    if (isolated) {
        dc.rectangle(area);
        dc.setSource(&intermediate);
        dc.setOperator(ink_css_blend_to_cairo_operator(_mix_blend_mode));
        dc.fill();
    }
    return RENDER_OK;
}

void
DrawingGroup::_clipItem(DrawingContext &dc, Geom::IntRect const &area)
{
    for (auto & i : _children) {
        i.clip(dc, area);
    }
}

DrawingItem *
DrawingGroup::_pickItem(Geom::Point const &p, double delta, unsigned flags)
{
    for (auto & i : _children) {
        DrawingItem *picked = i.pick(p, delta, flags);
        if (picked) {
            return _pick_children ? picked : this;
        }
    }
    return nullptr;
}

bool
DrawingGroup::_canClip()
{
    return true;
}

bool is_drawing_group(DrawingItem *item)
{
    return dynamic_cast<DrawingGroup *>(item) != nullptr;
}

} // end namespace Inkscape

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
