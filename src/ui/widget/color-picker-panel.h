// SPDX-License-Identifier: GPL-2.0-or-later
//
// Created by Michael Kowalski on 5/27/24.
//
// This is a widget hosting ColorPages and adding a color plate/wheel on top.
// It also injects a row with color eye dropper, rgb edit and color type selctor.
// This is a component used to implement https://gitlab.com/inkscape/ux/-/issues/246

#ifndef COLOR_PICKER_PANEL_H
#define COLOR_PICKER_PANEL_H

#include <gtkmm/grid.h>

#include "colors/color-set.h"
#include "colors/color.h"

namespace Inkscape::UI::Widget {

class ColorPickerPanel : public Gtk::Grid {
public:
    // color plate type - rectangular, color wheel, no plate (only sliders)
    enum PlateType {Rect, Circle, None};
    static std::unique_ptr<ColorPickerPanel> create(Colors::Space::Type space, PlateType type, std::shared_ptr<Colors::ColorSet> color);

    virtual void set_color(const Colors::Color& color) = 0;
    virtual void set_picker_type(Colors::Space::Type type) = 0;
    virtual void set_plate_type(PlateType plate) = 0;
    // current color page
    // virtual ColorPage& get_color_page() = 0;
};

} // namespace

#endif //COLOR_PICKER_PANEL_H
