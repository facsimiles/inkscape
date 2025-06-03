// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Ruler widget. Indicates horizontal or vertical position of a cursor in a specified widget.
 *
 * Copyright (C) 2019, 2023 Tavmjong Bah
 *               2022 Martin Owens
 *
 * Rewrite of the 'C' ruler code which came originally from Gimp.
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 *
 */

#include <cmath>
#include <sigc++/functors/mem_fun.h>
#include <cairomm/context.h>
#include <glibmm/ustring.h>
#include <giomm/menu.h>
#include <giomm/menuitem.h>
#include <gdkmm/general.h>
#include <gtkmm/binlayout.h>
#include <gtkmm/drawingarea.h>
#include <gtkmm/eventcontrollermotion.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/popovermenu.h>
#include <gtkmm/snapshot.h>

#include "ink-ruler.h"
#include "inkscape.h"
#include "ui/containerize.h"
#include "ui/controller.h"
#include "ui/popup-menu.h"
#include "ui/themes.h"
#include "ui/util.h"
#include "util/units.h"

namespace Inkscape::UI::Widget {

// Half width of pointer triangle.
constexpr double half_width = 5.0;

Ruler::Ruler(Gtk::Orientation orientation)
    : Glib::ObjectBase{"InkRuler"}
    , _orientation{orientation}
    , _popover{create_context_menu()}
{
    set_name("InkRuler");
    add_css_class(_orientation == Gtk::Orientation::HORIZONTAL ? "horz" : "vert");
    containerize(*this);
    set_layout_manager(Gtk::BinLayout::create());

    auto const motion = Gtk::EventControllerMotion::create();
    motion->signal_motion().connect([this, &motion = *motion](auto &&...args) { return on_motion(motion, args...); });
    add_controller(motion);

    auto const click = Gtk::GestureClick::create();
    click->set_button(3); // right
    click->signal_pressed().connect(Controller::use_state([this](auto &, auto &&...args) { return on_click_pressed(args...); }, *click));
    add_controller(click);

    auto prefs = Inkscape::Preferences::get();
    _watch_prefs = prefs->createObserver("/options/ruler/show_bbox", sigc::mem_fun(*this, &Ruler::on_prefs_changed));
    on_prefs_changed();

    INKSCAPE.themecontext->getChangeThemeSignal().connect(sigc::track_object([this] { css_changed(nullptr); }, *this));
}

Ruler::~Ruler() = default;

void Ruler::on_prefs_changed()
{
    auto prefs = Inkscape::Preferences::get();
    _sel_visible = prefs->getBool("/options/ruler/show_bbox", true);

    redraw_ruler();
}

// Set display unit for ruler.
void Ruler::set_unit(Inkscape::Util::Unit const *unit)
{
    if (_unit != unit) {
        _unit = unit;
        redraw_ruler();
        _scale_tile_node.reset();
    }
}

// Set range for ruler, update ticks.
void Ruler::set_range(double lower, double upper)
{
    if (_lower != lower || _upper != upper) {
        _lower = lower;
        _upper = upper;
        _max_size = _upper - _lower;
        if (_max_size == 0) {
            _max_size = 1;
        }
        redraw_ruler();
    }
}

/**
 * Set the location of the currently selected page.
 */
void Ruler::set_page(double lower, double upper)
{
    if (_page_lower != lower || _page_upper != upper) {
        _page_lower = lower;
        _page_upper = upper;
        redraw_ruler();
    }
}

/**
 * Set the location of the currently selected page.
 */
void Ruler::set_selection(double lower, double upper)
{
    if (_sel_lower != lower || _sel_upper != upper) {
        _sel_lower = lower;
        _sel_upper = upper;
        redraw_ruler();
    }
}

// Add a widget (i.e. canvas) to monitor.
void Ruler::set_track_widget(Gtk::Widget &widget)
{
    assert(!_track_widget_controller);
    _track_widget_controller = Gtk::EventControllerMotion::create();
    _track_widget_controller->set_propagation_phase(Gtk::PropagationPhase::TARGET);
    _track_widget_controller->signal_motion().connect([this] (auto &&...args) { return on_motion(*_track_widget_controller, args...); }, false); // before
    widget.add_controller(_track_widget_controller);
}

void Ruler::clear_track_widget()
{
    assert(_track_widget_controller);
    _track_widget_controller->get_widget()->remove_controller(_track_widget_controller);
    _track_widget_controller = {};
}

// Draws marker in response to motion events from canvas. Position is defined in ruler pixel
// coordinates. The routine assumes that the ruler is the same width (height) as the canvas. If
// not, one could use Gtk::Widget::translate_coordinates() to convert the coordinates.
void Ruler::on_motion(Gtk::EventControllerMotion &motion, double x, double y)
{
    // This may come from a widget other than `this`, so translate to accommodate border, etc.
    auto const widget = motion.get_widget();
    double drawing_x{}, drawing_y{};
    widget->translate_coordinates(*this, std::lround(x), std::lround(y), drawing_x, drawing_y);

    double const position = _orientation == Gtk::Orientation::HORIZONTAL ? drawing_x : drawing_y;
    if (position == _position) {
        return;
    }

    _position = position;
    queue_draw();
}

Gtk::EventSequenceState Ruler::on_click_pressed(int, double x, double y)
{
    UI::popup_at(*_popover, *this, x, y);
    return Gtk::EventSequenceState::CLAIMED;
}

static double safe_frac(double x)
{
    return x - std::floor(x);
}

void Ruler::draw_ruler(Glib::RefPtr<Gtk::Snapshot> const &snapshot)
{
    auto const dims = Geom::IntPoint{get_width(), get_height()};

    // aparallel is the longer dimension of the ruler; aperp shorter.
    auto const [aparallel, aperp] = _orientation == Gtk::Orientation::HORIZONTAL
        ? std::pair{dims.x(), dims.y()}
        : std::pair{dims.y(), dims.x()};

    // Color in page indication box
    if (auto const interval = Geom::IntInterval(std::round(_page_lower), std::round(_page_upper)) & Geom::IntInterval{0, aparallel}) {
        Geom::IntRect rect;
        if (_orientation == Gtk::Orientation::HORIZONTAL) {
            rect = {interval->min(), 0, interval->max(), aperp};
        } else {
            rect = {0, interval->min(), aperp, interval->max()};
        }
        snapshot->append_color(_page_fill, geom_to_gtk(rect));
    }

    // Draw bottom/right line of ruler
    Geom::IntRect edge_rect;
    if (_orientation == Gtk::Orientation::HORIZONTAL) {
        edge_rect = {0, aperp - 1, aparallel, aperp};
    } else {
        edge_rect = {aperp - 1, 0, aperp, aparallel};
    }
    snapshot->append_color(_foreground, geom_to_gtk(edge_rect));

    double const abs_size = std::abs(_max_size);
    int const sign = _max_size >= 0 ? 1 : -1;

    // Figure out scale. Largest ticks must be far enough apart to fit largest text in vertical ruler.
    // We actually require twice the distance.
    int scale = std::ceil(abs_size); // Largest number
    Glib::ustring const scale_text = std::to_string(scale);
    int const digits = scale_text.length() + 1; // Add one for negative sign.
    int const minimum = digits * _font_size * 2;

    double const pixels_per_unit = aparallel / abs_size;

    auto ruler_metric = _unit->getUnitMetric();
    if (!ruler_metric) {
        // User warning already done in Unit code.
        return;
    }

    unsigned scale_index;
    for (scale_index = 0; scale_index < ruler_metric->ruler_scale.size() - 1; ++scale_index) {
        if (ruler_metric->ruler_scale[scale_index] * pixels_per_unit > minimum) {
            break;
        }
    }

    // Now we find out what is the subdivide index for the closest ticks we can draw
    unsigned divide_index;
    for (divide_index = 0; divide_index < ruler_metric->subdivide.size() - 1; ++divide_index) {
        if (ruler_metric->ruler_scale[scale_index] * pixels_per_unit < 5 * ruler_metric->subdivide[divide_index + 1]) {
            break;
        }
    }

    int const subdivisions = ruler_metric->subdivide[divide_index];
    double const units_per_major = ruler_metric->ruler_scale[scale_index];
    double const pixels_per_major = pixels_per_unit * units_per_major;
    double const pixels_per_tick = pixels_per_major / subdivisions;

    // Figure out which cached render nodes to invalidate.
    if (!_params) {
        _params = LastRenderParams{
            .aparallel = aparallel,
            .aperp = aperp,
            .divide_index = divide_index,
            .pixels_per_tick = pixels_per_tick,
            .pixels_per_major = pixels_per_major
        };
    } else {
        auto update = [] (auto src, auto &dst, auto&... to_reset) {
            if (src != dst) {
                dst = src;
                (to_reset.reset(), ...);
            }
        };
        auto update_approx = [] (auto src, auto &dst, auto&... to_reset) {
            if (!Geom::are_near(src, dst)) {
                dst = src;
                (to_reset.reset(), ...);
            }
        };
        update(aparallel, _params->aparallel, _shadow_node, _scale_node);
        update(aperp, _params->aperp, _scale_tile_node);
        update(divide_index, _params->divide_index, _scale_tile_node);
        update_approx(pixels_per_tick, _params->pixels_per_tick, _scale_tile_node);
        update_approx(pixels_per_major, _params->pixels_per_major, _scale_node);
    }
    if (!_scale_tile_node) {
        _scale_node.reset(); // _scale_node contains _scale_tile_node
    }

    // Draw a shadow which overlaps any previously painted object.
    if (!_shadow_node) {
        Geom::IntRect shadow_rect;
        Geom::IntPoint end_point;
        static constexpr int gradient_size = 4;
        if (_orientation == Gtk::Orientation::HORIZONTAL) {
            shadow_rect = {0, 0, aparallel, gradient_size};
            end_point = {0, gradient_size};
        } else {
            shadow_rect = {0, 0, gradient_size, aparallel};
            end_point = {gradient_size, 0};
        }
        auto const stops = create_cubic_gradient(_shadow, change_alpha(_shadow, 0.0), Geom::Point(0, 0.5), Geom::Point(0.5, 1));
        auto shadow_snapshot = gtk_snapshot_new();
        gtk_snapshot_append_linear_gradient(
            shadow_snapshot,
            geom_to_gtk(shadow_rect).gobj(),
            geom_to_gtk(Geom::IntPoint{}).gobj(),
            geom_to_gtk(end_point).gobj(),
            stops.data(),
            stops.size()
        );
        _shadow_node = RenderNodePtr{gtk_snapshot_free_to_node(shadow_snapshot)};
    }
    gtk_snapshot_append_node(snapshot->gobj(), _shadow_node.get());

    snapshot->push_clip(geom_to_gtk(Geom::IntRect{{}, dims}));

    // Build a single scale tile, i.e. one major tick.
    if (!_scale_tile_node) {
        auto scale_tile = gtk_snapshot_new();

        for (int i = 0; i < subdivisions; i++) {
            // Position of tick
            double position = std::round(i * pixels_per_tick);

            // Height of tick
            int size = aperp - 7;
            for (int j = divide_index; j > 0; --j) {
                if (i % ruler_metric->subdivide[j] == 0) {
                    break;
                }
                size = size / 2 + 1;
            }

            // Draw ticks
            Geom::Rect rect;
            if (_orientation == Gtk::Orientation::HORIZONTAL) {
                rect = Geom::Rect(position, aperp - size, position + 1, aperp - 1);
            } else {
                rect = Geom::Rect(aperp - size, position, aperp - 1, position + 1);
            }
            gtk_snapshot_append_color(scale_tile, _foreground.gobj(), geom_to_gtk(rect).gobj());
        }

        _scale_tile_node = RenderNodePtr{gtk_snapshot_free_to_node(scale_tile)};
    }

    // Glue scale tiles together.
    // Note: We can't use a repeat node for this, because then the ticks will either be blurry or inaccurate.
    if (!_scale_node) {
        auto scale_tiles = gtk_snapshot_new();

        for (int i = 0; ; i++) {
            if (i > 0) {
                int const pos = std::round(i * pixels_per_major);
                if (pos >= aparallel + pixels_per_major) {
                    break;
                }
                int const lastpos = std::round((i - 1) * pixels_per_major);
                int const shift = pos - lastpos;
                auto const translate = _orientation == Gtk::Orientation::HORIZONTAL
                    ? Geom::IntPoint(shift, 0)
                    : Geom::IntPoint(0, shift);
                gtk_snapshot_translate(scale_tiles, geom_to_gtk(translate).gobj());
            }
            gtk_snapshot_append_node(scale_tiles, _scale_tile_node.get());
        }

        _scale_node = RenderNodePtr{gtk_snapshot_free_to_node(scale_tiles)};
    }

    // Render the scale with a shift.
    int const shift = -std::round(safe_frac(_lower * sign / units_per_major) * pixels_per_major);
    auto const translate = _orientation == Gtk::Orientation::HORIZONTAL
        ? Geom::Point(shift, 0)
        : Geom::Point(0, shift);
    snapshot->save();
    snapshot->translate(geom_to_gtk(translate));
    gtk_snapshot_append_node(snapshot->gobj(), _scale_node.get());

    // Find first and last major ticks
    int const start = std::floor(_lower * sign / units_per_major);
    int const end   = std::floor(_upper * sign / units_per_major);

    // Draw text for major ticks.
    for (int i = start; i <= end; ++i) {
        int const label_value = std::round(i * units_per_major * sign);
        double const position = std::round((i - start) * pixels_per_major);
        bool const rotate = _orientation != Gtk::Orientation::HORIZONTAL;
        auto const layout = create_pango_layout(std::to_string(label_value));

        int text_width{};
        int text_height{};
        layout->get_pixel_size(text_width, text_height);
        if (rotate) {
            std::swap(text_width, text_height);
        }

        // Align text to pixel
        int x = position + 3;
        int y = 3;
        if (rotate) {
            std::swap(x, y);
        }

        // Create label text or retrieve from cache. (Note: This cache is never pruned.)
        auto &label_node = _label_nodes[label_value];
        if (!label_node) {
            auto label = gtk_snapshot_new();
            gtk_snapshot_append_layout(label, layout->gobj(), _foreground.gobj());
            label_node = RenderNodePtr{gtk_snapshot_free_to_node(label)};
        }

        snapshot->save();
        snapshot->translate(Gdk::Graphene::Point(x, y));
        if (rotate) {
            snapshot->translate(Gdk::Graphene::Point(0.0, text_height));
            snapshot->rotate(-90);
        }
        gtk_snapshot_append_node(snapshot->gobj(), label_node.get());
        snapshot->restore();
    }

    snapshot->restore();

    // Draw a selection bar
    if (_sel_lower != _sel_upper && _sel_visible) {
        constexpr auto radius = 3.0;
        constexpr auto line_width = 2.0;
        auto const delta = _sel_upper - _sel_lower;
        auto const dxy = delta > 0 ? radius : -radius;
        double sy0 = _sel_lower;
        double sy1 = _sel_upper;
        double sx0 = std::floor(aperp * 0.7);
        double sx1 = sx0;

        if (_orientation == Gtk::Orientation::HORIZONTAL) {
            std::swap(sy0, sx0);
            std::swap(sy1, sx1);
        }

        if (std::abs(delta) > 2 * radius) {
            Geom::Rect rect;
            if (_orientation == Gtk::Orientation::HORIZONTAL) {
                auto const y = std::round(sy0 - line_width / 2);
                rect = Geom::Rect(sx0 + dxy, y, sx1 - dxy, y + line_width);
            } else {
                auto const x = std::round(sx0 - line_width / 2);
                rect = Geom::Rect(x, sy0 + dxy, x + line_width, sy1 - dxy);
            }
            snapshot->append_color(_select_stroke, geom_to_gtk(rect).gobj());
        }

        static auto const path = [] {
            auto builder = gsk_path_builder_new();
            gsk_path_builder_add_circle(builder, Gdk::Graphene::Point{0.0, 0.0}.gobj(), radius);
            return gsk_path_builder_free_to_path(builder);
        }();

        static auto const stroke = [] {
            return gsk_stroke_new(line_width);
        }();

        // Markers
        for (auto pt : {Geom::Point(sx0, sy0), Geom::Point(sx1, sy1)}) {
            snapshot->save();
            snapshot->translate(geom_to_gtk(pt));
            gtk_snapshot_append_fill(snapshot->gobj(), path, GSK_FILL_RULE_WINDING, _select_fill.gobj());
            gtk_snapshot_append_stroke(snapshot->gobj(), path, stroke, _select_stroke.gobj());
            snapshot->restore();
        }
    }

    snapshot->pop();
}

// Draw position marker, we use doubles here.
void Ruler::draw_marker(Glib::RefPtr<Gtk::Snapshot> const &snapshot)
{
    static auto const path = [] {
        auto builder = gsk_path_builder_new();
        gsk_path_builder_move_to(builder, 0, 0);
        gsk_path_builder_line_to(builder, -half_width, -half_width);
        gsk_path_builder_line_to(builder, half_width, -half_width);
        gsk_path_builder_close(builder);
        return gsk_path_builder_free_to_path(builder);
    }();

    auto const pos = _orientation == Gtk::Orientation::HORIZONTAL
        ? Geom::Point(_position, get_height())
        : Geom::Point(get_width(), _position);
    snapshot->save();
    snapshot->translate(geom_to_gtk(pos));
    if (_orientation != Gtk::Orientation::HORIZONTAL) {
        snapshot->rotate(-90);
    }
    gtk_snapshot_append_fill(snapshot->gobj(), path, GSK_FILL_RULE_WINDING, _foreground.gobj());
    snapshot->restore();
}

void Ruler::snapshot_vfunc(Glib::RefPtr<Gtk::Snapshot> const &snapshot)
{
    if (!_ruler_node) {
        auto ruler = gtk_snapshot_new();
        draw_ruler(Glib::wrap_gtk_snapshot(ruler, true));
        _ruler_node = RenderNodePtr{gtk_snapshot_free_to_node(ruler)};
    }
    gtk_snapshot_append_node(snapshot->gobj(), _ruler_node.get());
    draw_marker(snapshot);
}

// Update ruler on style change (font-size, etc.)
void Ruler::css_changed(GtkCssStyleChange *change)
{
    // Cache all our colors to speed up rendering.
    _foreground = get_color();
    _font_size = get_font_size(*this);

    _shadow = get_color_with_class(*this, "shadow");
    _page_fill = get_color_with_class(*this, "page");

    add_css_class("selection");
    _select_fill = get_color_with_class(*this, "background");
    _select_stroke = get_color_with_class(*this, "border");
    remove_css_class("selection");

    redraw_ruler();
    _shadow_node.reset();
    _label_nodes.clear();
}

/**
 * Return a contextmenu for the ruler
 */
std::unique_ptr<Gtk::Popover> Ruler::create_context_menu()
{
    auto unit_menu = Gio::Menu::create();

    for (auto &pair : Util::UnitTable::get().units(Inkscape::Util::UNIT_TYPE_LINEAR)) {
        auto unit = pair.second.abbr;
        Glib::ustring action_name = "doc.set-display-unit('" + unit + "')";
        auto item = Gio::MenuItem::create(unit, action_name);
        unit_menu->append_item(item);
    }

    auto popover = std::make_unique<Gtk::PopoverMenu>(unit_menu);
    popover->set_parent(*this);
    popover->set_autohide(true);
    return popover;
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
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
