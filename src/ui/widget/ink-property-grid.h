// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * InkPropertyGrid: grid that can hold list of properties in form of label and editing widgets
 * with support for a single and two-column layout.
 */
/*
 * Author:
 *   Michael Kowalski
 *
 * Copyright (C) 2024 Michael Kowalski
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INK_PROPERTY_GRID_H
#define INK_PROPERTY_GRID_H

#include <gtkmm/grid.h>
#include <gtkmm/label.h>
#include <gtkmm/sizegroup.h>

#include "bin.h"

namespace Inkscape::UI::Widget {

class InkPropertyGrid : public Bin {
public:
    InkPropertyGrid();
    ~InkPropertyGrid() override = default;

    // add a property row to the grid; label and values are optional; widget1 is expected whereas widget2
    // can be specified if this is (potentially) 2-column property (like dimensions width and height)
    Gtk::Widget* add_property(Gtk::Label* label, Gtk::Widget* button1, Gtk::Widget* widget1, Gtk::Widget* widget2, Gtk::Widget* button2 = nullptr, int margin = 2);
    Gtk::Widget* add_property(const std::string& label, Gtk::Widget* button1, Gtk::Widget* widget1, Gtk::Widget* widget2, Gtk::Widget* button2 = nullptr, int margin = 2);
    // leave a gap before adding new row; used to indicate new group of properties
    Gtk::Widget* add_gap(int size = 8);
    // add a widget to the grid that will occupy both columns
    void add_row(Gtk::Widget* widget, Gtk::Widget* button = nullptr, bool whole_row = true, int margin = 2);
    void add_row(const std::string& label, Gtk::Widget* widget, Gtk::Widget* button = nullptr, int margin = 2);

private:
    void construct();
    void set_single_column(bool single);

    Gtk::Grid _grid;
    int _row = 0;
    int _min_width = 0;
    bool _single_column = false;
    Glib::RefPtr<Gtk::SizeGroup> _field_width = Gtk::SizeGroup::create(Gtk::SizeGroup::Mode::HORIZONTAL);
    Glib::RefPtr<Gtk::SizeGroup> _field_height = Gtk::SizeGroup::create(Gtk::SizeGroup::Mode::VERTICAL);
};

}

#endif //INK_PROPERTY_GRID_H
