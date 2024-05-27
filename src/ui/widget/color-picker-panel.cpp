// SPDX-License-Identifier: GPL-2.0-or-later
//
// Created by Michael Kowalski on 5/27/24.
//

#include "color-picker-panel.h"

#include <glib/gi18n.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/aspectframe.h>
#include <gtkmm/box.h>
#include <gtkmm/stack.h>
#include <gtkmm/togglebutton.h>

#include <utility>
#include "color-plate.h"
#include "color-preview.h"
#include "color-page.h"
#include "color-wheel.h"
#include "icon-combobox.h"
#include "ink-spin-button.h"
#include "colors/color.h"
#include "colors/manager.h"
#include "colors/spaces/base.h"

namespace Inkscape::UI::Widget {

using namespace Colors;

struct Panel: Gtk::Grid {
    Panel(Space::Type type, std::shared_ptr<ColorSet> color_set, std::string plate_type):
        _type(type), _color_set(std::move(color_set)), _plate_type(std::move(plate_type)), _preview(0) {
        set_column_spacing(0);
        set_row_spacing(0);
        _preview.set_border_radius(3);
        _preview.set_frame(true);
        create_color_page(type);
        update_color();

        _color_changed = _color_set->signal_changed.connect([this]() { update_color(); });
    }

    void update_color() {
        if (!_color_set->isEmpty()) {
            auto color = _color_set->getAverage();
            _preview.setRgba32(color.toRGBA());
            _rgb_edit.set_text(rgba_to_hex(color.toRGBA(), false));
            if (_plate) { _plate->set_color(color); }
        }
    }

    void create_color_page(Space::Type type) {
        if (_page) {
            // detach existing page
            remove(*_page);
        }
        if (_plate) {
            // removed managed wheel
            remove(_plate->get_widget());
            _plate = nullptr;
        }
        auto space = Manager::get().find(type);
        _page = std::make_unique<ColorPage>(space, _color_set);
        _page->show_expander(false);
        if (_plate_type == "circle") {
            _plate = _page->create_color_wheel(type, true);
        }
        else if (_plate_type == "rect") {
            _plate = _page->create_color_wheel(type, false);
        }
        update_color();
    }

    void set_type(Space::Type type) {
        if (_type == type) return;

        // recreate color page
        if (_page) {
            create_color_page(type);
            attach(*_page, 1, _page_row);
            if (_plate) {
                _plate->get_widget().set_expand();
                attach(_plate->get_widget(), 0, 0, 4);
            }
        }

        _type = type;
    }

    int add_widgets() {
        int row = 0;
        if (_plate) {
            _plate->get_widget().set_expand();
            attach(_plate->get_widget(), 0, row++, 2);
        }
        else {
            set_margin_top(4);
        }

        auto box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
        box->set_spacing(1);
        box->set_vexpand(false);
        box->set_valign(Gtk::Align::CENTER);
        constexpr int size = 48;
        _preview.set_size_request(size, size);
        _preview.set_checkerboard_tile_size(size / 3);
        _preview.set_margin_top(10); // to optically center it vertically, push it down
        box->append(_preview);
        box->append(_rgb_edit);
        box->set_margin(3);
        box->set_margin_end(8); // distance from labels
        _rgb_edit.set_has_frame(false);
        _rgb_edit.set_max_width_chars(3);
        _rgb_edit.add_css_class("small-entry");
        _rgb_edit.set_alignment(0.5f);
        _rgb_changed = _rgb_edit.signal_changed().connect([this]() {
            // parse "#rrggbb" and other formats and update color...
            // todo
        });
        attach(*box, 0, row);

        _page_row = row;
        attach(*_page, 1, row++);
        return row;
    }

    // color type this picker is working in
    Space::Type _type = Space::Type::NONE;
    std::shared_ptr<ColorSet> _color_set;
    std::string _plate_type;
    std::unique_ptr<ColorPage> _page;
    int _page_row = 0;
    bool _disc = true;
    ColorWheel* _plate = nullptr;
    ColorPreview _preview;
    Gtk::Entry _rgb_edit;
    auto_connection _color_changed;
    auto_connection _rgb_changed;
};

class ColorPickerPanelImpl : public ColorPickerPanel {
public:
    ColorPickerPanelImpl(const Glib::ustring& title, std::shared_ptr<ColorSet> color);
    void set_color(const Color& color) override;

    void select_color_type(Space::Type type);
    void _select_color_type(Space::Type type);
    void create_page(Space::Type type, const std::string& name);
    Panel* current_panel() {
        return dynamic_cast<Panel*>(_stack.get_visible_child());
    }

    Gtk::Box _header;
    enum Type {Rect, Circle, Sliders};
    Gtk::ToggleButton _rect_btn;
    Gtk::ToggleButton _circle_btn;
    Gtk::ToggleButton _sliders_btn;
    Gtk::Label _title;
    IconComboBox _spaces;
    Gtk::Stack _stack;
    std::unique_ptr<Panel> _circle_picker;
    std::unique_ptr<Panel> _rect_picker;
    std::unique_ptr<Panel> _slider_picker;
    // selected color space for this color picker
    Space::Type _color_space = Space::Type::NONE;
    std::shared_ptr<ColorSet> _color_set;
};

std::unique_ptr<ColorPickerPanel> ColorPickerPanel::create() {
    return std::make_unique<ColorPickerPanelImpl>(Glib::ustring(), nullptr);
}

std::unique_ptr<ColorPickerPanel> ColorPickerPanel::create(const Glib::ustring& title, std::shared_ptr<ColorSet> color) {
    return std::make_unique<ColorPickerPanelImpl>(title, color);
}

ColorPickerPanelImpl::ColorPickerPanelImpl(const Glib::ustring& title, std::shared_ptr<ColorSet> color): _color_set(std::move(color)) {
    if (!_color_set) _color_set = std::make_shared<ColorSet>();

    set_row_spacing(0);
    set_column_spacing(0);
    int row = 0;
    _header.set_spacing(2);
    for (auto [btn, id, icon] :
        {std::tuple{&_rect_btn, "rect", "color-picker-rect"},
        {&_circle_btn, "circle", "color-picker-circle"},
        {&_sliders_btn, "input", "color-picker-input"}}) {

        btn->set_has_frame(false);
        // btn->set_image_from_icon_name(icon);
        btn->set_icon_name(icon);
        // btn->set_child(*Gtk::make_managed<Gtk::Image>(Gio::ThemedIcon::create(icon)));
        if (btn != &_rect_btn) btn->set_group(_rect_btn);
        auto name = id;
        btn->signal_toggled().connect([this, name]() {
            auto type = static_cast<Space::Type>(_spaces.get_active_row_id());
            create_page(type, name);
            // switch UI
            _stack.set_visible_child(name);
        });
    }
    _spaces.set_has_frame(false);
    for (auto&& meta : Colors::Manager::get().spaces(Space::Traits::Picker)) {
        _spaces.add_row(meta->getIcon(), meta->getName(), int(meta->getType()));
    }
    _spaces.refilter();
    _spaces.signal_changed().connect([this](int id) {
        auto type = static_cast<Space::Type>(id);
        if (type != Space::Type::NONE) {
            select_color_type(type);
        }
    });

    _header.append(_rect_btn);
    _header.append(_circle_btn);
    _header.append(_sliders_btn);
    _title.set_hexpand();
    _title.set_xalign(0.5);
    _title.set_text(title);
    _header.append(_title);
    _spaces.set_halign(Gtk::Align::END);
    _header.append(_spaces);
    attach(_header, 0, row++);

    _stack.set_hhomogeneous();
    _stack.set_vhomogeneous(false);
    attach(_stack, 0, row++);

    //todo - remember both
    _spaces.set_active_by_id(int(Space::Type::HSL));
    _rect_btn.set_active();
    // _select_color_type(Space::Type::HSL);
}

void ColorPickerPanelImpl::create_page(Space::Type type, const std::string& name) {
    if (name == "rect") {
        if (!_rect_picker) {
            _rect_picker = std::make_unique<Panel>(type, _color_set, name);
            _rect_picker->add_widgets();
            _stack.add(*_rect_picker, name);
        }
    }
    else if (name == "circle") {
        if (!_circle_picker) {
            _circle_picker = std::make_unique<Panel>(type, _color_set, name);
            _circle_picker->add_widgets();
            _stack.add(*_circle_picker, name);
        }
    }
    else if (name == "input") {
        if (!_slider_picker) {
            _slider_picker = std::make_unique<Panel>(type, _color_set, name);
            _slider_picker->add_widgets();
            _stack.add(*_slider_picker, "input");
        }
    }
    else {
        g_error("Missing case in create_page.");
    }
}

void ColorPickerPanelImpl::set_color(const Color& color) {
    _color_set->set(color);
    if (auto page = current_panel()) {
        page->set_type(_color_space);
        // page->set_current_color(color);
    }
}

void ColorPickerPanelImpl::select_color_type(Space::Type type) {
    if (_color_space == type) return;

    _select_color_type(type);
}

void ColorPickerPanelImpl::_select_color_type(Space::Type type) {
    _color_space = type;

    if (auto page = dynamic_cast<Panel*>(_stack.get_visible_child())) {
        page->set_type(_color_space);
    }

    auto maybe_color = _color_set->get();
    if (!maybe_color) {
        _color_set->set(Color{type, {0,0,0,0}});
    }
    auto color = *_color_set->get();
    // set_color(color);
    // //todo handle failure
}

} // namespace
