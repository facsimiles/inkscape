// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Author:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Ralf Stephan <ralf@ark.in-berlin.de>
 *   Michael Kowalski
 *
 * Copyright (C) 2001-2005 Authors
 * Copyright (C) 2001 Ximian, Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "ui/widget/color-preview.h"

#include "display/cairo-utils.h"
#include "ui/util.h"

namespace Inkscape::UI::Widget {

ColorPreview::ColorPreview(std::uint32_t const rgba)
    : _rgba{rgba}
{
    set_name("ColorPreview");
    set_draw_func(sigc::mem_fun(*this, &ColorPreview::draw_func));
    setStyle(_style);
}

void ColorPreview::setRgba32(std::uint32_t const rgba) {
    if (_rgba == rgba && !_pattern) return;

    _rgba = rgba;
    _pattern = {};
    queue_draw();
}

void ColorPreview::setPattern(Cairo::RefPtr<Cairo::Pattern> pattern) {
    if (_pattern == pattern) return;

    _pattern = pattern;
    _rgba = 0;
    queue_draw();
}

Geom::Rect round_rect(const Cairo::RefPtr<Cairo::Context>& ctx, Geom::Rect rect, double radius) {
    auto x = rect.left();
    auto y = rect.top();
    auto width = rect.width();
    auto height = rect.height();
    ctx->arc(x + width - radius, y + radius, radius, -M_PI_2, 0);
    ctx->arc(x + width - radius, y + height - radius, radius, 0, M_PI_2);
    ctx->arc(x + radius, y + height - radius, radius, M_PI_2, M_PI);
    ctx->arc(x + radius, y + radius, radius, M_PI, 3 * M_PI_2);
    ctx->close_path();
    return rect.shrunkBy(1);
}

Cairo::RefPtr<Cairo::Pattern> create_checkerboard_pattern(double tx, double ty) {
    auto pattern = Cairo::RefPtr<Cairo::Pattern>(new Cairo::Pattern(ink_cairo_pattern_create_checkerboard(), true));
    pattern->set_matrix(Cairo::translation_matrix(tx, ty));
    return pattern;
}

void ColorPreview::draw_func(Cairo::RefPtr<Cairo::Context> const &cr,
                             int const widget_width, int const widget_height)
{
    double width = widget_width;
    double height = widget_height;
    auto x = 0.0;
    auto y = 0.0;
    double radius = _style == Simple ? 0.0 : 2.0;
    double degrees = M_PI / 180.0;
    auto rect = Geom::Rect(x, y, x + width, y + height);

    std::uint32_t outline_color = 0x00000000;
    std::uint32_t border_color = 0xffffff00;

    auto style = get_style_context();
    Gdk::RGBA bgnd;
    bool found = style->lookup_color("theme_bg_color", bgnd);
    // use theme background color as a proxy for dark theme; this method is fast, which is important here
    auto dark_theme = found && get_luminance(bgnd) <= 0.5;
    auto state = get_state_flags();
    auto disabled = (state & Gtk::StateFlags::INSENSITIVE) == Gtk::StateFlags::INSENSITIVE;
    auto backdrop = (state & Gtk::StateFlags::BACKDROP) == Gtk::StateFlags::BACKDROP;
    if (dark_theme) {
        std::swap(outline_color, border_color);
    }

    if (_style == Outlined) {
        // outside outline
        rect = round_rect(cr, rect, radius--);
        // opacity of outside outline is reduced
        int alpha = disabled || backdrop ? 0x2f : 0x5f;
        ink_cairo_set_source_color(cr->cobj(), Colors::Color(outline_color | alpha));
        cr->fill();

        // inside border
        rect = round_rect(cr, rect, radius--);
        ink_cairo_set_source_color(cr->cobj(), Colors::Color(border_color, false));
        cr->fill();
    }

    if (_pattern) {
        // draw pattern-based preview
        round_rect(cr, rect, radius);

        // checkers first
        auto checkers = create_checkerboard_pattern(-x, -y);
        cr->set_source(checkers);
        cr->fill_preserve();

        cr->set_source(_pattern);
        cr->fill();
    }
    else {
        // color itself
        auto color = Colors::Color(_rgba);
        auto opacity = color.stealOpacity();
        // if preview is disabled, render colors with reduced saturation and intensity
        if (disabled) {
            color = make_disabled_color(color, dark_theme);
        }

        width = rect.width() / 2;
        height = rect.height();
        x = rect.min().x();
        y = rect.min().y();

        // solid on the right
        cairo_new_sub_path(cr->cobj());
        cairo_line_to(cr->cobj(), x + width, y);
        cairo_line_to(cr->cobj(), x + width, y + height);
        cairo_arc(cr->cobj(), x + radius, y + height - radius, radius, 90 * degrees, 180 * degrees);
        cairo_arc(cr->cobj(), x + radius, y + radius, radius, 180 * degrees, 270 * degrees);
        cairo_close_path(cr->cobj());
        ink_cairo_set_source_color(cr->cobj(), color);
        cr->fill();

        // semi-transparent on the left
        x += width;
        cairo_new_sub_path(cr->cobj());
        cairo_arc(cr->cobj(), x + width - radius, y + radius, radius, -90 * degrees, 0 * degrees);
        cairo_arc(cr->cobj(), x + width - radius, y + height - radius, radius, 0 * degrees, 90 * degrees);
        cairo_line_to(cr->cobj(), x, y + height);
        cairo_line_to(cr->cobj(), x, y);
        cairo_close_path(cr->cobj());
        if (opacity < 1.0) {
            auto checkers = create_checkerboard_pattern(-x, -y);
            cr->set_source(checkers);
            cr->fill_preserve();
        }
        color.setOpacity(opacity);
        ink_cairo_set_source_color(cr->cobj(), color);
        cr->fill();
    }

    if (_indicator != None) {
        constexpr double side = 7.5;
        constexpr double line = 1.5; // 1.5 pixles b/c it's a diagonal line, so 1px is too thin
        const auto right = rect.right();
        const auto bottom = rect.bottom();
        if (_indicator == Swatch) {
            // draw swatch color indicator - a black corner
            cr->move_to(right, bottom - side);
            cr->line_to(right, bottom - side + line);
            cr->line_to(right - side + line, bottom);
            cr->line_to(right - side, bottom);
            cr->set_source_rgb(1, 1, 1); // white separator
            cr->fill();
            cr->move_to(right, bottom - side + line);
            cr->line_to(right, bottom);
            cr->line_to(right - side + line, bottom);
            cr->set_source_rgb(0, 0, 0); // black
            cr->fill();
        }
        else if (_indicator == SpotColor) {
            // draw spot color indicator - a black dot
            cr->move_to(right, bottom);
            cr->line_to(right, bottom - side);
            cr->line_to(right - side, bottom);
            cr->set_source_rgb(1, 1, 1); // white background
            cr->fill();
            constexpr double r = 2;
            cr->arc(right - r, bottom - r, r, 0, 2*M_PI);
            cr->set_source_rgb(0, 0, 0);
            cr->fill();
        }
        else {
            assert(false);
        }
    }
}

void ColorPreview::setStyle(Style style) {
    _style = style;
    if (style == Simple) {
        add_css_class("simple");
    }
    else {
        remove_css_class("simple");
    }
    queue_draw();
}

void ColorPreview::setIndicator(Indicator indicator) {
    if (_indicator != indicator) {
        _indicator = indicator;
        queue_draw();
    }
}

} // namespace Inkscape::UI::Widget

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
