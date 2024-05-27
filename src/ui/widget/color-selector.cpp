// SPDX-License-Identifier: GPL-2.0-or-later

#include "color-selector.h"
#include <array>
#include <giomm/themedicon.h>
#include <gtkmm/box.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/enums.h>
#include <gtkmm/image.h>
#include <gtkmm/object.h>
#include <gtkmm/stack.h>
#include <memory>
#include "ui/widget/icon-combobox.h"
#include "colors/color.h"
#include "colors/color-set.h"

namespace Inkscape::UI::Widget {

ColorSelector::ColorSelector() : Gtk::Box(Gtk::Orientation::VERTICAL) {}

class ColorSelectorImpl : public ColorSelector {
public:
    ColorSelectorImpl();

    Gtk::ToggleButton _rect;
    Gtk::ToggleButton _wheel;
    Gtk::ToggleButton _sliders;
    IconComboBox _switch = IconComboBox(true, true);
    Gtk::Stack _stack;
    SelectedColor _selected_color = std::make_shared<Colors::ColorSet>();
    bool _no_alpha = false;
};

std::unique_ptr<ColorSelector> ColorSelector::create() {
    return std::make_unique<ColorSelectorImpl>();
}

ColorSelectorImpl::ColorSelectorImpl() {
    auto header = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);

    auto color_buttons = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    color_buttons->set_halign(Gtk::Align::START);
    for (auto [btn, icon] : std::to_array(
        { std::pair{&_rect, "color-switch-rect"}, {&_wheel, "color-switch-wheel"}, {&_sliders, "color-switch-sliders"}})) {

        btn->set_image_from_icon_name(icon);
        if (btn != &_rect) btn->set_group(_rect);
        btn->set_has_frame(false);
        color_buttons->append(*btn);
        auto radio_btn = btn;
        btn->signal_toggled().connect([=,this](){
            if (radio_btn->get_active()) {
                // switch color selector type: rect, wheel,, or sliders
                //todo
            }
        });
    }
    header->append(*color_buttons);

    for (auto&& picker : get_color_pickers()) {
        if (auto selector_widget = picker.factory->createWidget(_selected_color, _no_alpha)) {
            _stack.add(*selector_widget);
            //todo: picker visibility
            _switch.add_row(picker.icon, picker.label, int(picker.mode));
        }
    }
    _switch.refilter();
    _switch.set_halign(Gtk::Align::END);
    _switch.set_hexpand();
    dynamic_cast<Gtk::ToggleButton&>(*_switch.get_first_child()).set_has_frame(false);
    header->append(_switch);

    append(*header);
    append(_stack);
}

} // namespace
