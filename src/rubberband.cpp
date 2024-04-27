// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Rubberbanding selector.
 *
 * Author:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Jon A. Cruz <jon@joncruz.org>
 *
 * Copyright (C) 1999-2002 Lauris Kaplinski
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "desktop.h"

#include "rubberband.h"

#include "2geom/path.h"

#include "display/cairo-utils.h"
#include "display/curve.h"
#include "display/control/canvas-item-bpath.h"
#include "display/control/canvas-item-rect.h"

#include "ui/widget/canvas.h" // autoscroll

Inkscape::Rubberband *Inkscape::Rubberband::_instance = nullptr;

Inkscape::Rubberband::Rubberband(SPDesktop *dt)
    : _desktop(dt)
{
    _touchpath_curve = new SPCurve();
}

void Inkscape::Rubberband::delete_canvas_items()
{
    _rect.reset();
    _touchpath.reset();
}

Geom::Path Inkscape::Rubberband::getPath() const
{
    g_assert(_started);
    if (_mode == Rubberband::Mode::TOUCHPATH) {
        return _path * _desktop->w2d();
    }
    return Geom::Path(*getRectangle());
}

std::vector<Geom::Point> Inkscape::Rubberband::getPoints() const
{
    return _path.nodes();
}

void Inkscape::Rubberband::start(SPDesktop *d, Geom::Point const &p, bool tolerance)
{
    _desktop = d;

    _start = p;
    _started = true;
    _moved = false;

    auto prefs = Inkscape::Preferences::get();
    _tolerance = tolerance ? prefs->getIntLimited("/options/dragtolerance/value", 0, 0, 100) : 0.0;

    _touchpath_curve->reset();
    _touchpath_curve->moveto(p);

    _path = Geom::Path(_desktop->d2w(p));

    delete_canvas_items();
}

void Inkscape::Rubberband::stop()
{
    _started = false;
    _moved = false;

    set_default_mode(); // Can't set default style as well, that causes a race condition

    _touchpath_curve->reset();
    _path.clear();

    delete_canvas_items();
}

void Inkscape::Rubberband::move(Geom::Point const &p)
{
    if (!_started) 
        return;

    if (!_moved) {
        if (Geom::are_near(_start, p, _tolerance/_desktop->current_zoom()))
            return;
    }

    _end = p;
    _moved = true;
    _desktop->getCanvas()->enable_autoscroll();
    _touchpath_curve->lineto(p);

    Geom::Point next = _desktop->d2w(p);
    // we want the points to be at most 0.5 screen pixels apart,
    // so that we don't lose anything small;
    // if they are farther apart, we interpolate more points
    auto prev = _path.finalPoint();
    if (Geom::L2(next-prev) > 0.5) {
        int subdiv = 2 * (int) round(Geom::L2(next-prev) + 0.5);
        for (int i = 1; i <= subdiv; i ++) {
            _path.appendNew<Geom::LineSegment>(prev + ((double)i/subdiv) * (next - prev));
        }
    } else {
        _path.appendNew<Geom::LineSegment>(next);
    }

    if (_touchpath) _touchpath->set_visible(false);
    if (_rect) _rect->set_visible(false);

    switch (_mode) {
        case Inkscape::Rubberband::Mode::RECT:
            if (!_rect) {
                _rect = make_canvasitem<CanvasItemRect>(_desktop->getCanvasControls());
                _rect->set_stroke(_style.stroke);
                _rect->set_fill(_style.fill);
                _rect->set_shadow(0xffffffff, 0); // Not a shadow
                _rect->set_dashed(_style.is_dashed);
            }
            _rect->set_rect(Geom::Rect(_start, _end));
            _rect->set_visible(true);
            break;
        case Inkscape::Rubberband::Mode::TOUCHRECT:
            if (!_rect) {
                _rect = make_canvasitem<CanvasItemRect>(_desktop->getCanvasControls());
                _rect->set_stroke(_style.stroke);
                _rect->set_fill(_style.fill);
                _rect->set_fill_pattern(_style.fill_pattern);
                _rect->set_shadow(0xffffffff, 0); // Not a shadow
                _rect->set_dashed(_style.is_dashed);
            }
            _rect->set_rect(Geom::Rect(_start, _end));
            _rect->set_visible(true);
            break;
        case Inkscape::Rubberband::Mode::TOUCHPATH:
            if (!_touchpath) {
                _touchpath = make_canvasitem<CanvasItemBpath>(_desktop->getCanvasControls()); // Should be sketch?
                _touchpath->set_stroke(_style.stroke);
                _touchpath->set_stroke_outset(_style.stroke_outset);
                _touchpath->set_stroke_width(_style.stroke_width);
                _touchpath->set_fill(_style.fill, SP_WIND_RULE_EVENODD);
                _touchpath->set_fill_pattern(_style.fill_pattern);
                if (_style.is_dashed) {
                    _touchpath->set_dashes(std::vector{4.0});
                }
            }
            _touchpath->set_bpath(_touchpath_curve);
            _touchpath->set_visible(true);
            break;
        default:
            break;
    }
}

void Inkscape::Rubberband::set_mode_with_style(Inkscape::Rubberband::Mode mode, Inkscape::Rubberband::Style &style) {
    set_mode(mode);
    set_style(std::move(style));
}

void Inkscape::Rubberband::set_mode_with_default_style(Inkscape::Rubberband::Mode mode) {
    set_mode(mode);
    set_style(get_default_style(mode));
}

/**
 * @return The default style for each of the rubberband modes. The caller should consider the
 * returned style as a base and then modify the members as required.
 */
Inkscape::Rubberband::Style Inkscape::Rubberband::get_default_style(Inkscape::Rubberband::Mode mode) {
    Rubberband::Style style{};

    switch (mode) {
        case Rubberband::Mode::TOUCHRECT:
            // TODO: Collect all places where this pattern is used, and cache it somehow
            static auto _pattern = ink_cairo_pattern_create_slanting_stripes(0x277fff1a);
            style = Rubberband::Style{
                .fill = 0x277fff1a,
                .fill_pattern = _pattern,
            };
            break;
        case Rubberband::Mode::TOUCHPATH:
            style = Rubberband::Style{
                .fill = 0x0,
            };
            break;
        case Rubberband::Mode::RECT: // RECT is default
            break;
        default:
            break;
    }
    return style;
}

/**
 * @return Rectangle in desktop coordinates
 */
Geom::OptRect Inkscape::Rubberband::getRectangle() const
{
    if (!_started) {
        return Geom::OptRect();
    }

    return Geom::Rect(_start, _end);
}

Inkscape::Rubberband *Inkscape::Rubberband::get(SPDesktop *desktop)
{
    if (!_instance) {
        _instance = new Inkscape::Rubberband(desktop);
    }

    return _instance;
}

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
