//
// Created by Michael Kowalski on 9/7/24.
//

#include "swatch-editor.h"

namespace Inkscape::UI::Widget {

constexpr auto prefs = "/popup/swatch-fill";

SwatchEditor::SwatchEditor(Colors::Space::Type space):
    Gtk::Box(Gtk::Orientation::VERTICAL),
    _panel(Dialog::SwatchesPanel::Popup, prefs),
    _colors(new Colors::ColorSet()),
    _color_picker(ColorPickerPanel::create(space, get_plate_type_preference(prefs, ColorPickerPanel::None), _colors))
{
    add_css_class("SwatchEditor");
    _panel.add_css_class("SwatchList");
    append(_panel);
    append(*_color_picker);
}

void SwatchEditor::set_desktop(SPDesktop* desktop) {
    _panel.setDesktop(desktop);
}

void SwatchEditor::select_vector(SPGradient* vector) {
}

SPGradient* SwatchEditor::get_selected_vector() const {
    return 0;
}

void SwatchEditor::set_color_picker_plate(ColorPickerPanel::PlateType type) {
    _color_picker->set_plate_type(type);
    set_plate_type_preference(prefs, type);
}

ColorPickerPanel::PlateType SwatchEditor::get_color_picker_plate() const {
    return _color_picker->get_plate_type();
}

} // namespace
