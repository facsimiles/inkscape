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

#include <cairo.h>
#include <cairomm/context.h>
#include <cairomm/matrix.h>
#include <cairomm/pattern.h>
#include <gdkmm/rgba.h>
#include <gtkmm/enums.h>
#include <gtkmm/window.h>
#include <sigc++/functors/mem_fun.h>
#include <gtkmm/drawingarea.h>
#include "display/cairo-utils.h"
#include "colors/color.h"
#include "colors/spaces/enum.h"
#include <2geom/rect.h>
#include "ui/util.h"
#include "util/drawing-utils.h"
#include "util/theme-utils.h"

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

static Cairo::RefPtr<Cairo::Pattern> _create_checkerboard_pattern(Gtk::Widget& w, double tx, double ty, int size) {
    auto [col1, col2] = Util::get_checkerboard_colors(w, false);
    auto pattern = ::create_checkerboard_pattern(col1, col2, size);
    // auto pattern = Cairo::RefPtr<Cairo::Pattern>(new Cairo::Pattern(ink_cairo_pattern_create_checkerboard(), true));
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
    if (_radius >= 0) radius = _radius;
    double degrees = M_PI / 180.0;
    auto rect = Geom::Rect(x, y, x + width, y + height);

    std::uint32_t outline_color = 0x00000000;
    std::uint32_t border_color = 0xffffff00;

    bool dark_theme = Util::is_current_theme_dark(*this);
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
        ink_cairo_set_source_rgba32(cr->cobj(), outline_color | alpha);
        cr->fill();

        // inside border
        rect = round_rect(cr, rect, radius--);
        ink_cairo_set_source_rgba32(cr->cobj(), border_color | 0xff);
        cr->fill();
    }

    if (_pattern) {
        // draw pattern-based preview
        round_rect(cr, rect, radius);

        // checkers first
        auto checkers = _create_checkerboard_pattern(*this, -x, -y, _checkerboard_tile_size);
        cr->set_source(checkers);
        cr->fill_preserve();

        cr->set_source(_pattern);
        cr->fill();
    }
    else {
        // color itself
        auto rgba = _rgba;
        auto alpha = rgba & 0xff;
        // if preview is disabled, render colors with reduced saturation and intensity
        if (disabled) {
            auto color = Colors::Color(_rgba);
            auto hsl = *color.converted(Colors::Space::Type::HSLUV);
            // reduce saturation and lightness/darkness (on dark/light theme)
            double lf = 0.35; // lightness factor - 35% of lightness
            double sf = 0.30; // saturation factor - 30% of saturation
            // for both light and dark themes the idea it to compress full range of color lightness (0..1)
            // to a narrower range to convey subdued look of disabled widget (that's the lf * l part);
            // then we move the lightness floor to 0.70 for light theme and 0.20 for dark theme:
            auto saturation = hsl.get(1) * sf;
            auto lightness = lf * hsl.get(2) + (dark_theme ? 0.20 : 0.70); // new lightness in 0..1 range
            hsl.set(1, saturation);
            hsl.set(2, lightness);
            auto rgb = *hsl.converted(Colors::Space::Type::RGB);
            rgba = rgb.toRGBA();
        }

        width = rect.width() / 2;
        height = rect.height();
        x = rect.min().x();
        y = rect.min().y();

        // solid on the right
        auto solid = rgba | 0xff;
        cairo_new_sub_path(cr->cobj());
        cairo_line_to(cr->cobj(), x + width, y);
        cairo_line_to(cr->cobj(), x + width, y + height);
        cairo_arc(cr->cobj(), x + radius, y + height - radius, radius, 90 * degrees, 180 * degrees);
        cairo_arc(cr->cobj(), x + radius, y + radius, radius, 180 * degrees, 270 * degrees);
        cairo_close_path(cr->cobj());
        ink_cairo_set_source_rgba32(cr->cobj(), solid);
        cr->fill();

        // semi-transparent on the left
        x += width;
        cairo_new_sub_path(cr->cobj());
        cairo_arc(cr->cobj(), x + width - radius, y + radius, radius, -90 * degrees, 0 * degrees);
        cairo_arc(cr->cobj(), x + width - radius, y + height - radius, radius, 0 * degrees, 90 * degrees);
        cairo_line_to(cr->cobj(), x, y + height);
        cairo_line_to(cr->cobj(), x, y);
        cairo_close_path(cr->cobj());
        if (alpha < 0xff) {
            auto checkers = _create_checkerboard_pattern(*this, -x, -y, _checkerboard_tile_size);
            cr->set_source(checkers);
            cr->fill_preserve();
        }
        ink_cairo_set_source_rgba32(cr->cobj(), rgba);
        cr->fill();
    }

    // Draw fill/stroke indicators.
    if (_is_fill || _is_stroke) {
        auto color = Colors::Color(_rgba);
        double const lightness = Colors::get_perceptual_lightness(color);
        auto [gray, alpha] = Colors::get_contrasting_color(lightness);
        cr->set_source_rgba(gray, gray, gray, alpha);

        // Scale so that the square -1...1 is the biggest possible square centred in the widget.
        auto w = rect.width();
        auto h = rect.height();
        auto minwh = std::min(w, h);
        cr->translate((w - minwh) / 2.0, (h - minwh) / 2.0);
        cr->scale(minwh / 2.0, minwh / 2.0);
        cr->translate(1.0, 1.0);

        if (_is_fill) {
            cr->arc(0.0, 0.0, 0.35, 0.0, 2 * M_PI);
            cr->fill();
        }

        if (_is_stroke) {
            cr->set_fill_rule(Cairo::Context::FillRule::EVEN_ODD);
            cr->arc(0.0, 0.0, 0.65, 0.0, 2 * M_PI);
            cr->arc(0.0, 0.0, 0.5, 0.0, 2 * M_PI);
            cr->fill();
        }
    }

    if (_indicator != None) {
        constexpr double side = 7.5;
        constexpr double line = 1.5; // 1.5 pixles b/c it's a diagonal line, so 1px is too thin
        const auto right = rect.right();
        const auto bottom = rect.bottom();
        if (_indicator & Swatch) {
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
        else if (_indicator & SpotColor) {
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

        if (_indicator & (LinearGradient | RadialGradient)) {
            auto s = 3.0; // arrow size
            auto h = s / 2; // half size
            auto w = width - 2*s - 2;
            auto cx = std::round(x + width / 2);
            auto cy = std::round(y + height / 2);

            if (_indicator & LinearGradient) {
                auto start = Geom::Point(x + 1 + s, cy);
                const Geom::Point path_linear[] = {
                    {0, h}, {-s, -h}, {s, -h}, {0, h}, {w, 0}, {0, h}, {s, -h}, {-s, -h}, {0, h}
                };
                auto p = start;
                cr->move_to(p.x(), p.y());
                for (auto& d: path_linear) {
                    p += d;
                    cr->line_to(p.x(), p.y());
                }
            }
            else {
                auto start = Geom::Point(cx, y + 1 + s);
                const Geom::Point path_radial[] = {
                    {h, 0}, {-h, -s}, {-h, s}, {h, 0}, {0, cy - s - 1},
                    {cx - s - 1, 0}, {0, h}, {s, -h}, {-s, -h}, {0, h}, {-(cx - s - 1), 0}
                };
                auto p = start;
                cr->move_to(p.x(), p.y());
                for (auto& d : path_radial) {
                    p += d;
                    cr->line_to(p.x(), p.y());
                }
            }
            cr->close_path();
            cr->set_line_width(2.0);
            cr->set_miter_limit(10);
            cr->set_source_rgba(1, 1, 1, 1.0);
            cr->stroke_preserve();
            cr->set_line_width(1.0);
            cr->set_source_rgba(0, 0, 0, 1);
            cr->stroke_preserve();
            // cr->set_source_rgba(1, 1, 1, 1);
            cr->fill();
        }
    }

    if (_style == Simple && _frame) {
        Util::draw_standard_border(cr, rect, dark_theme, radius, get_scale_factor(), false);
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

void ColorPreview::set_frame(bool frame) {
    if (_frame != frame) {
        _frame = frame;
        queue_draw();
    }
}

void ColorPreview::set_border_radius(int radius) {
    if (_radius != radius) {
        _radius = radius;
        queue_draw();
    }
}

void ColorPreview::set_checkerboard_tile_size(unsigned size) {
    if (_checkerboard_tile_size != size) {
        _checkerboard_tile_size = size;
        queue_draw();
    }
}

void ColorPreview::set_fill(bool on) {
    _is_fill = on;
    queue_draw();
}

void ColorPreview::set_stroke(bool on) {
    _is_stroke = on;
    queue_draw();
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
