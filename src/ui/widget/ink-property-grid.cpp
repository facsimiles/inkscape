// SPDX-License-Identifier: GPL-2.0-or-later

#include "ink-property-grid.h"

#include <gtkmm/box.h>
#include <gtkmm/label.h>

namespace Inkscape::UI::Widget {

InkPropertyGrid::InkPropertyGrid() {
    construct();
}

void InkPropertyGrid::construct() {
    set_name("InkPropertyGrid");
    set_child(_grid);

    connectBeforeResize([this](int width, int height, int baseline) {
        auto m = measure(Gtk::Orientation::HORIZONTAL);
        if (!_single_column && m.sizes.minimum < _min_width - 1) {
            _min_width = m.sizes.minimum + 1;
        }
        auto single = width <= _min_width;
        if (single != _single_column) {
            // introduce hysteresis to avoid flickering
            if (!single && abs(width - _min_width) < 2) return;
        }
        set_single_column(single);
    });
}

// grid columns:
constexpr int COL_LABEL    = 0; // property name
constexpr int COL_BUTTON_1 = 1; // button in front of property (like a padlock, scale lock, etc)
constexpr int COL_FILED_1  = 2; // property widget
constexpr int COL_BUTTON_2 = 3; // button at the end of property (like a reset/clear)

WidgetGroup InkPropertyGrid::add_property(Gtk::Label* label, Gtk::Widget* button1, Gtk::Widget* w1, Gtk::Widget* w2, Gtk::Widget* btn, int margin) {
    WidgetGroup group;

    if (label) {
        group.add(label);
        label->set_margin(margin);
        _field_height->add_widget(*label);
        label->set_halign(Gtk::Align::START);
        label->set_valign(Gtk::Align::START);
        _grid.attach(*label, COL_LABEL, _row, button1 ? 1 : 2);
    }
    if (button1) {
        group.add(button1);
        button1->set_margin(margin);
        button1->set_margin_end(0);
        button1->set_valign(Gtk::Align::CENTER);
        _grid.attach(*button1, COL_BUTTON_1, _row);
    }
    if (w1) {
        group.add(w1);
        w1->set_margin(margin);
        w1->set_hexpand();
        _field_width->add_widget(*w1);
        _field_height->add_widget(*w1);
    }
    if (w2) {
        if (w2->get_halign() == Gtk::Align::START) {
            auto box = Gtk::make_managed<Gtk::Box>();
            box->append(*w2);
            w2 = box;
        }
        group.add(w2);
        w2->set_margin(margin);
        w2->set_hexpand();
        _field_width->add_widget(*w2);
        _field_height->add_widget(*w2);
    }
    else {
        w2 = Gtk::make_managed<Gtk::Box>();
        group.add(w2);
        w2->set_hexpand();
        w2->set_margin_start(margin);
        w2->set_margin_end(margin);
        _field_width->add_widget(*w2);
    }

    if (w1 && w2) {
        auto box = Gtk::make_managed<Gtk::Box>();
        // group = box;
        box->add_css_class("fields");
        box->append(*w1);
        box->append(*w2);
        _grid.attach(*box, COL_FILED_1, _row);
    }
    else if (w1) {
        _grid.attach(*w1, COL_FILED_1, _row);
    }
    if (btn) {
        group.add(btn);
        btn->set_margin(margin);
        btn->set_margin_start(0);
        btn->set_margin_end(0);
        _grid.attach(*btn, COL_BUTTON_2, _row);
    }

    ++_row;

    auto m = measure(Gtk::Orientation::HORIZONTAL);
    _min_width = m.sizes.minimum + 1;
    return group;
}

WidgetGroup InkPropertyGrid::add_property(const std::string& label, Gtk::Widget* button1, Gtk::Widget* widget1, Gtk::Widget* widget2, Gtk::Widget* button2, int margin) {
    auto l = Gtk::make_managed<Gtk::Label>(label);
    l->set_halign(Gtk::Align::START);
    return add_property(l, button1, widget1, widget2, button2, margin);
}

Gtk::Widget* InkPropertyGrid::add_gap(int size) {
    auto gap = Gtk::make_managed<Gtk::Box>();
    gap->set_size_request(1, size);
   _grid.attach(*gap, COL_LABEL, _row++);
    return gap;
}

void InkPropertyGrid::add_row(Gtk::Widget* widget, Gtk::Widget* button, bool whole_row, int margin) {
    if (!widget) return;

    widget->set_margin(margin);
    _grid.attach(*widget, whole_row ? COL_LABEL : COL_FILED_1, _row, whole_row ? 3 : 2);

    if (button) {
        button->set_margin(margin);
        button->set_margin_start(0);
        button->set_margin_end(0);
        _grid.attach(*button, COL_BUTTON_2, _row);
    }

    ++_row;
}

WidgetGroup InkPropertyGrid::add_row(const std::string& label, Gtk::Widget* widget, Gtk::Widget* button, int margin) {
    WidgetGroup group;
    if (!label.empty()) {
        auto l = Gtk::make_managed<Gtk::Label>(label);
        l->set_halign(Gtk::Align::START);
        l->set_margin(margin);
        _grid.attach(*l, COL_LABEL, _row);
        group.add(l);
    }

    if (widget) {
        widget->set_margin(margin);
        _grid.attach(*widget, COL_FILED_1, _row);
        group.add(widget);
    }

    if (button) {
        button->set_margin(margin);
        button->set_margin_start(0);
        button->set_margin_end(0);
        _grid.attach(*button, COL_BUTTON_2, _row);
        group.add(button);
    }

    ++_row;
    return group;
}

void InkPropertyGrid::set_single_column(bool single) {
    if (_single_column == single) return;

    _single_column = single;

    for (int row = 0; row < _row; ++row) {
        if (auto box = dynamic_cast<Gtk::Box*>(_grid.get_child_at(COL_FILED_1, row))) {
            if (box->has_css_class("fields")) {
                box->set_orientation(single ? Gtk::Orientation::VERTICAL : Gtk::Orientation::HORIZONTAL);
            }
        }
    }
}

}
