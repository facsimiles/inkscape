// SPDX-License-Identifier: GPL-2.0-or-later
//
// Created by Michael Kowalski on 9/7/24.
//

#ifndef SWATCH_EDITOR_H
#define SWATCH_EDITOR_H

#include "color-picker-panel.h"
#include "edit-operation.h"
#include "object/sp-gradient.h"
#include "ui/dialog/swatches.h"

namespace Inkscape::UI::Widget {

class SwatchEditor : public Gtk::Box {
public:
    SwatchEditor(Colors::Space::Type space);

    void set_desktop(SPDesktop* desktop);
    void select_vector(SPGradient* vector);
    SPGradient* get_selected_vector() const;
    void set_color_picker_plate(ColorPickerPanel::PlateType type);
    ColorPickerPanel::PlateType get_color_picker_plate() const;
    sigc::signal<void (SPGradient*, EditOperation, SPGradient*)> signal_changed() const { return _signal_changed; }
    sigc::signal<void (SPGradient*, const Colors::Color&)> signal_color_changed() const { return _signal_color_changed; }

private:
    Dialog::SwatchesPanel _panel;
    std::shared_ptr<Colors::ColorSet> _colors;
    std::unique_ptr<ColorPickerPanel> _color_picker;
    SPGradient* _vector = nullptr;
    sigc::signal<void (SPGradient*, EditOperation, SPGradient*)> _signal_changed;
    sigc::signal<void (SPGradient*, const Colors::Color&)> _signal_color_changed;
};

} // namespace

#endif //SWATCH_EDITOR_H
