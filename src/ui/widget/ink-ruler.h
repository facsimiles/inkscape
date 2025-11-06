// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Ruler widget. Indicates horizontal or vertical position of a cursor in a specified widget.
 *
 * Copyright (C) 2019 Tavmjong Bah
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 *
 */

#ifndef INKSCAPE_UI_WIDGET_RULER_H
#define INKSCAPE_UI_WIDGET_RULER_H

#include "preferences.h"
#include "ui/widget-vfuncs-class-init.h"
#include "util/delete-with.h"

namespace Cairo { class Context; }

namespace Gtk {
class EventControllerMotion;
class GestureClick;
class Popover;
} // namespace Gtk

namespace Inkscape::Util { class Unit; }

namespace Inkscape::UI::Widget {

using RenderNodePtr = std::unique_ptr<GskRenderNode, Util::Deleter<gsk_render_node_unref>>;

class Ruler
    : public WidgetVfuncsClassInit
    , public Gtk::Widget
{
public:
    Ruler(Gtk::Orientation orientation);
    ~Ruler() override;

    void set_unit(Inkscape::Util::Unit const *unit);
    void set_range(double lower, double upper);
    void set_page(double lower, double upper);
    void set_selection(double lower, double upper);

    void set_track_widget(Gtk::Widget &widget);
    void clear_track_widget();

private:
    void draw_ruler(Glib::RefPtr<Gtk::Snapshot> const &snapshot);
    void draw_marker(Glib::RefPtr<Gtk::Snapshot> const &snapshot);
    void snapshot_vfunc(Glib::RefPtr<Gtk::Snapshot> const &snapshot) override;

    void css_changed(GtkCssStyleChange *) override;
    void on_prefs_changed();

    void on_motion(Gtk::EventControllerMotion &motion, double x, double y);
    Gtk::EventSequenceState on_click_pressed(int n_press, double x, double y);

    std::unique_ptr<Gtk::Popover> create_context_menu();

    Inkscape::PrefObserver _watch_prefs;
    std::unique_ptr<Gtk::Popover> _popover;
    Gtk::Orientation const _orientation;
    Inkscape::Util::Unit const *_unit = nullptr;
    double _lower = 0;
    double _upper = 1000;
    double _position = 0;
    double _max_size = 1000;

    // Page block
    double _page_lower = 0.0;
    double _page_upper = 0.0;

    // Selection block
    double _sel_lower = 0.0;
    double _sel_upper = 0.0;
    double _sel_visible = true;

    Glib::RefPtr<Gtk::EventControllerMotion> _track_widget_controller;

    // Cached style properties
    Gdk::RGBA _foreground;
    Gdk::RGBA _major;
    Gdk::RGBA _minor;
    int _font_size{};
    Gdk::RGBA _page_fill;
    Gdk::RGBA _select_fill;
    Gdk::RGBA _select_stroke;
    Gdk::RGBA _select_bgnd;

    // Cached render nodes.
    RenderNodePtr _scale_tile_node;
    RenderNodePtr _scale_node;
    std::map<int, RenderNodePtr> _label_nodes;
    RenderNodePtr _ruler_node;
    void redraw_ruler()
    {
        _ruler_node.reset();
        queue_draw();
    }

    struct LastRenderParams
    {
        int aparallel;
        int aperp;
        unsigned divide_index;
        double pixels_per_tick;
        double pixels_per_major;
    };
    std::optional<LastRenderParams> _params;
};

} // namespace Inkscape::UI::Widget

#endif // INKSCAPE_UI_WIDGET_RULER_H

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
