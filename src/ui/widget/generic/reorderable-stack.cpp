// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Build a stack of buttons who's order in the stack is the main value.
 *//*
 * Copyright (C) 2025 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "reorderable-stack.h"

namespace Inkscape::UI::Widget {

ReorderableStack::ReorderableStack(Gtk::Orientation orientation)
{
    set_name("ReorderableStack");
    append(_tabs);
    _tabs.set_hexpand();
    _tabs.set_orientation(orientation);
    _tabs.set_draw_handle();

    _tabs.add_css_class("border-box");
    _tabs.add_css_class("entry-box");

    _tabs.set_show_labels(TabStrip::ShowLabels::Always);
    _tabs.set_rearranging_tabs(TabStrip::Rearrange::Internally);

    _tabs.signal_tab_rearranged().connect([this](int, int) {
        _signal_values_changed.emit();
    });
    _tabs.set_new_tab_popup(nullptr);
}

/**
 * Add an option to the stack, this should be done on construction.
 */
void ReorderableStack::add_option(std::string const &label, std::string const &icon, std::string const &tooltip, int value)
{
    // Build a row
    auto row = _tabs.add_tab(label, icon);
    row->set_tooltip_text(tooltip);
    row->set_hexpand(true);
    _rows.emplace_back(row, value);

}

/**
 * Show or hide one of the values in the stack.
 */
void ReorderableStack::setVisible(int value, bool is_visible)
{
    for (auto &[widget, row_val] : _rows) {
        if (value == row_val) {
            widget->set_visible(is_visible);
            break;
        }
    }
}

/**
 * Set the order of the values as they are in the vector.
 */
void ReorderableStack::setValues(std::vector<int> const &values)
{
    std::vector<Gtk::Widget *> widgets;
    for (auto value : values) {
        auto it = std::find_if(_rows.begin(), _rows.end(),
            [value](const std::pair<Gtk::Widget *, int>& row){ return row.second == value;} );
        widgets.emplace_back(it->first);
    }
    _tabs.set_tabs_order(widgets);
}

/**
 * Get the order of the values as a vector.
 */
std::vector<int> ReorderableStack::getValues() const
{
    std::vector<int> values;
    for (auto tab : _tabs.get_tabs()) {
        auto it = std::find_if(_rows.begin(), _rows.end(),
            [tab](const std::pair<Gtk::Widget *, int>& row){ return row.first == tab;} );
        values.emplace_back(it->second);
    }
    return values;
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
// vim:filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99:
