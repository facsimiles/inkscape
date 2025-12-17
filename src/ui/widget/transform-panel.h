// SPDX-License-Identifier: GPL-2.0-or-later
//
// Created by Michael Kowalski on 12/12/25.
//

// TransformationPanel works in conjunction with InkPropertyGrid
// to present selection transformation widgets and buttons.

#ifndef INKSCAPE_TRANSFORM_PANEL_H
#define INKSCAPE_TRANSFORM_PANEL_H

#include <gtkmm/builder.h>

#include "desktop.h"
#include "widget-group.h"
#include "preferences.h"

namespace Gtk {
class Stack;
class Button;
class CheckButton;
}

namespace Inkscape::UI::Widget {
class TabStrip;
class InkPropertyGrid;
class InkSpinButton;

class TransformPanel {
public:
    TransformPanel();
    ~TransformPanel();

    // inject transform panel widgets into a grid
    void add_to_grid(InkPropertyGrid& grid);

    // show/hide all widgets
    void set_visible(bool show);

    // select page to show
    void set_page(int page);

    void set_desktop(SPDesktop* desktop);

    void update_ui(Selection& selection);

private:
    void show_page(int page);
    // reset all UI widgets to the default settings (or neutral, no-op settings)
    void reset_to_defaults();
    // apply transform
    void on_apply(bool duplicate);
    // fill matrix UI with identity matrix
    void clear_matrix();

    Glib::RefPtr<Gtk::Builder> _builder;
    InkSpinButton& _matrix_a;
    InkSpinButton& _matrix_b;
    InkSpinButton& _matrix_c;
    InkSpinButton& _matrix_d;
    InkSpinButton& _matrix_e;
    InkSpinButton& _matrix_f;
    InkSpinButton& _scale_x;
    InkSpinButton& _scale_y;
    InkSpinButton& _move_x;
    InkSpinButton& _move_y;
    InkSpinButton& _skew_x;
    InkSpinButton& _skew_y;
    InkSpinButton& _rotate;
    Gtk::CheckButton& _separate_transform;
    Gtk::CheckButton& _relative_move;
    Gtk::CheckButton& _current_matrix;
    Gtk::Stack& _replace_matrix;
    Gtk::Button& _link_scale_btn;
    Gtk::Button& _apply_btn;
    Gtk::Button& _duplicate_btn;
    WidgetGroup _tabs;
    WidgetGroup _transform_page;
    WidgetGroup _matrix_page;
    WidgetGroup _footer;
    TabStrip* _tab_strip = nullptr;
    Gtk::Widget* _tab_transform = nullptr;
    Gtk::Widget* _tab_matrix = nullptr;
    SPDesktop* _desktop = nullptr;
    Pref<int> _cur_page_pref{"/panels/transform/current-page"};
    Pref<bool> _replace_matrix_pref{"/panels/transform/replace-matrix"};
    Pref<bool> _linked_scale_pref{"/panels/transform/linked-scale"};
};

} // namespace

#endif //INKSCAPE_TRANSFORM_PANEL_H
