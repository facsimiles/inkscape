// SPDX-License-Identifier: GPL-2.0-or-later
//
// Created by Michael Kowalski on 5/27/24.
//

#ifndef COLOR_PICKER_PANEL_H
#define COLOR_PICKER_PANEL_H

#include <gtkmm/grid.h>

#include "colors/color-set.h"
#include "colors/color.h"

namespace Inkscape::UI::Widget {

class ColorPickerPanel : public Gtk::Grid {
public:
    static std::unique_ptr<ColorPickerPanel> create();
    static std::unique_ptr<ColorPickerPanel> create(const Glib::ustring& title, std::shared_ptr<Colors::ColorSet> color);

    virtual void set_color(const Colors::Color& color) = 0;

    enum Type {Rect, Circle, Sliders};
    virtual void set_picker_type(Type type) = 0;

    // virtual sigc::signal<void (const Colors::Color&)>& signal_color_changed() = 0;
};

} // namespace

#endif //COLOR_PICKER_PANEL_H
