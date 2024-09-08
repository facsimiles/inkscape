//
// Created by Michael Kowalski on 9/7/24.
//

#ifndef SWATCH_EDITOR_H
#define SWATCH_EDITOR_H

#include "color-picker-panel.h"
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

private:
    Dialog::SwatchesPanel _panel;
    std::shared_ptr<Colors::ColorSet> _colors;
    std::unique_ptr<ColorPickerPanel> _color_picker;
};

} // namespace

#endif //SWATCH_EDITOR_H
