// SPDX-License-Identifier: GPL-2.0-or-later
//
// Created by Michael Kowalski on 9/7/24.
//

#include "swatch-editor.h"

#include "gradient-chemistry.h"
#include "object/sp-stop.h"

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

    _panel.get_signal_operation().connect([this](auto action) {
        auto swatch = get_selected_vector();
        if (!swatch) return;

        if (action == EditOperation::Delete) {
            if (sp_can_delete_swatch(swatch->document, swatch)) {
                auto replacement = sp_find_replacement_swatch(swatch->document, swatch);
                _signal_changed.emit(swatch, action, replacement);
            }
        }
        else {
            _signal_changed.emit(swatch, action, nullptr);
        }
    });

    _colors->signal_changed.connect([this]() {
        auto swatch = get_selected_vector();
        if (!swatch) return;

        auto c = _colors->getAverage();
        _signal_color_changed.emit(swatch, c);
    });
}

void SwatchEditor::set_desktop(SPDesktop* desktop) {
    _panel.setDesktop(desktop);
    _vector = nullptr;
}

void SwatchEditor::select_vector(SPGradient* vector) {
    //todo
    _vector = vector;

    Color color(0x000000ff);
    if (vector && vector->hasStops()) {
        color = vector->getFirstStop()->getColor();
    }
    _color_picker->set_color(color);
}

SPGradient* SwatchEditor::get_selected_vector() const {
    //todo
    return _vector;
}

void SwatchEditor::set_color_picker_plate(ColorPickerPanel::PlateType type) {
    _color_picker->set_plate_type(type);
    set_plate_type_preference(prefs, type);
}

ColorPickerPanel::PlateType SwatchEditor::get_color_picker_plate() const {
    return _color_picker->get_plate_type();
}

} // namespace
