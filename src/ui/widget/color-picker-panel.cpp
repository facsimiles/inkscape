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

#include <string>
#include <utility>

#include "color-entry.h"
#include "color-plate.h"
#include "color-preview.h"
#include "color-page.h"
#include "color-wheel.h"
#include "icon-combobox.h"
#include "ink-spin-button.h"
#include "colors/color.h"
#include "colors/manager.h"
#include "colors/spaces/base.h"

const std::string spinner_pattern = "999.";
constexpr int ROW_PLATE = 0; // color plate, if any
constexpr int ROW_EDIT = 1;  // dropper, rgb edit box, color type selector
constexpr int ROW_PAGE = 3;  // color page with sliders

namespace Inkscape::UI::Widget {

using namespace Colors;

class ColorPickerPanelImpl : public ColorPickerPanel {
public:
    ColorPickerPanelImpl(Space::Type space, PlateType type, std::shared_ptr<ColorSet> color, bool with_expander);

    void update_color() {
        if (!_color_set->isEmpty()) {
            auto color = _color_set->getAverage();
            _preview.setRgba32(color.toRGBA());
            _rgb_edit.set_text(rgba_to_hex(color.toRGBA(), false));
            if (_plate) { _plate->set_color(color); }
        }
    }

    void remove_widgets() {
        if (_page) {
            _page->detach_page(_first_column, _last_column);
            // detach existing page
            remove(*_page);
            _page = nullptr;
        }
        if (_plate) {
            // removed managed wheel
            remove(_plate->get_widget());
            _plate = nullptr;
        }
    }

    void create_color_page(Space::Type type, PlateType plate_type) {
        auto space = Manager::get().find(type);
        _page = std::make_unique<ColorPage>(space, _color_set);
        _page->show_expander(false);
        _page->set_spinner_size_pattern(spinner_pattern);
        _page->attach_page(_first_column, _last_column);
        attach(*_page, 0, ROW_PAGE, 3);
        if (plate_type == Circle) {
            _plate = _page->create_color_wheel(type, true);
            _plate->get_widget().set_margin_bottom(4);
        }
        else if (plate_type == Rect) {
            _plate = _page->create_color_wheel(type, false);
        }
        if (_plate) {
            _plate->get_widget().set_expand();
            // counter internal padding reserved to show current color indicator; align plate with below widgets
            _plate->get_widget().set_margin_start(-4);
            _plate->get_widget().set_margin_end(-4);
            attach(_plate->get_widget(), 0, ROW_PLATE, 3);
        }
        update_color();
    }

    // void set_type(Space::Type type) {
    //     if (_space_type == type) return;
    //
    //     // recreate color page
    //     if (_page) {
    //         create_color_page(type);
    //         attach(*_page, 1, _page_row);
    //         if (_plate) {
    //             _plate->get_widget().set_expand();
    //             attach(_plate->get_widget(), 0, 0, 4);
    //         }
    //     }
    //
    //     _space_type = type;
    // }

    Gtk::Widget* add_gap(int size, int row) {
        auto gap = Gtk::make_managed<Gtk::Box>();
        gap->set_size_request(1, size);
        attach(*gap, 0, row);
        return gap;
    }

    // int add_widgets(const std::string& name) {
        // color preview with #rrggbb value
        // auto box = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::VERTICAL);
        // box->set_spacing(1);
        // box->set_vexpand(false);
        // box->set_valign(Gtk::Align::CENTER);
        // constexpr int size = 48;
        // _preview.set_size_request(size, size);
        // _preview.set_checkerboard_tile_size(size / 2 / 3);
        // _preview.set_margin_top(10); // to optically center it vertically, push it down
        // box->append(_preview);
        // box->append(_rgb_edit);
        // box->set_margin(3);
        // box->set_margin_end(8); // distance from labels
        // _rgb_edit.set_has_frame(false);
        // _rgb_edit.set_max_width_chars(3);
        // _rgb_edit.add_css_class("small-entry");
        // _rgb_edit.set_alignment(0.5f);
        // _rgb_changed = _rgb_edit.signal_changed().connect([this]() {
        //     // parse "#rrggbb" and other formats and update color...
        //     // todo
        // });
        // attach(*box, 0, row);

        // if (name == "circle") {
        //     _page->set_margin_bottom(5);
        // }
        // _page_row = row;
        // _page->set_spinner_size_pattern(spinner_pattern);
        // attach(*_page, 0, row++, 3);
        // return row;
    // }

    void set_color(const Color& color) override;
    void set_picker_type(Space::Type type) override;
    void set_plate_type(PlateType plate) override;

    void switch_page(Space::Type space, PlateType plate_type);
    // void _select_color_type(Space::Type type);
    // void set_color_plate(Space::Type type, const std::string& name);
    // Panel* current_panel() {
    //     return dynamic_cast<Panel*>(_stack.get_visible_child());
    // }

    Glib::RefPtr<Gtk::SizeGroup> _first_column = Gtk::SizeGroup::create(Gtk::SizeGroup::Mode::HORIZONTAL);
    Glib::RefPtr<Gtk::SizeGroup> _last_column = Gtk::SizeGroup::create(Gtk::SizeGroup::Mode::HORIZONTAL);
    // eye dropper - color picker
    Gtk::Button _dropper;
    // frame for RGB edit box
    Gtk::Box _frame;
    ColorPreview _preview;
    ColorEntry _rgb_edit;
    // color type space selector
    IconComboBox _spaces = IconComboBox(true, true);
    bool _with_expander = true;
    // color type this picker is working in
    Space::Type _space_type = Space::Type::NONE;
    std::shared_ptr<ColorSet> _color_set;
    PlateType _plate_type;
    std::unique_ptr<ColorPage> _page;
    // int _page_row = 0;
    // bool _disc = true;
    ColorWheel* _plate = nullptr;
    auto_connection _color_changed;
    auto_connection _rgb_changed;
};

std::unique_ptr<ColorPickerPanel> ColorPickerPanel::create(Colors::Space::Type space, PlateType type, std::shared_ptr<Colors::ColorSet> color) {
    return std::make_unique<ColorPickerPanelImpl>(space, type, color, false);
}

// std::unique_ptr<ColorPickerPanel> ColorPickerPanel::create() {
//     return std::make_unique<ColorPickerPanelImpl>(Glib::ustring(), nullptr, true);
// }
//
// std::unique_ptr<ColorPickerPanel> ColorPickerPanel::create(const Glib::ustring& title, std::shared_ptr<ColorSet> color, bool with_expander) {
//     return std::make_unique<ColorPickerPanelImpl>(title, color, with_expander);
// }

ColorPickerPanelImpl::ColorPickerPanelImpl(Space::Type space, PlateType type, std::shared_ptr<ColorSet> color, bool with_expander):
    _rgb_edit(color),
    _with_expander(with_expander),
    _space_type(space),
    _color_set(std::move(color)),
    _plate_type(type)
{

    if (!_color_set) _color_set = std::make_shared<ColorSet>();
    _color_set->signal_changed.connect([this]() {
        update_color(); });

    set_row_spacing(0);
    set_column_spacing(0);

    // list available color space types
    for (auto&& meta : Manager::get().spaces(Space::Traits::Picker)) {
        _spaces.add_row(meta->getIcon(), meta->getName(), int(meta->getType()));
    }
    _spaces.refilter();
    _spaces.set_tooltip_text(_("Select color picker type"));
    // Important: add "regular" class to render non-symbolic color icons;
    // otherwise they will be rendered black&white
    _spaces.add_css_class("regular");
    _spaces.signal_changed().connect([this](int id) {
        auto type = static_cast<Space::Type>(id);
        if (type != Space::Type::NONE) {
            set_picker_type(type);
        }
    });

    // color picker button
    _dropper.set_icon_name("color-picker");
    _first_column->add_widget(_dropper);
    _dropper.signal_clicked().connect([this]() {
        // pick color
    });
    // RGB edit box
    _frame.set_hexpand();
    _frame.set_spacing(4);
    _frame.add_css_class("border-box");
    // match frame size visually with color sliders width
    _frame.set_margin_start(8);
    _frame.set_margin_end(8);
    _preview.setStyle(ColorPreview::Simple);
    _preview.set_frame(true);
    _preview.set_border_radius(0);
    _preview.set_size_request(16, 16);
    _preview.set_checkerboard_tile_size(4);
    _preview.set_margin_start(3);
    _preview.set_halign(Gtk::Align::START);
    _preview.set_valign(Gtk::Align::CENTER);
    _frame.append(_preview);
    _rgb_edit.set_hexpand();
    _rgb_edit.set_has_frame(false);
    _rgb_edit.set_alignment(Gtk::Align::CENTER);
    _rgb_edit.add_css_class("small-entry");
    _frame.append(_rgb_edit);
    // color space type selector
    _spaces.set_halign(Gtk::Align::END);
    _last_column->add_widget(_spaces);

    Gtk::Widget* one_row[3] ={&_dropper, &_frame, &_spaces};
    for (auto widget : one_row) {
        widget->set_margin_top(4);
        widget->set_margin_bottom(4);
    }
    attach(_dropper, 0, ROW_EDIT);
    attach(_frame, 1, ROW_EDIT);
    attach(_spaces, 2, ROW_EDIT);

    create_color_page(space, type);
}

void ColorPickerPanelImpl::set_color(const Color& color) {
    _color_set->set(color);
}

void ColorPickerPanelImpl::set_picker_type(Space::Type type) {
    switch_page(type, _plate_type);
}

void ColorPickerPanelImpl::set_plate_type(PlateType plate) {
    if (plate == _plate_type) return;

    switch_page(_space_type, plate);
}

void ColorPickerPanelImpl::switch_page(Space::Type space, PlateType plate_type) {
    remove_widgets();
    create_color_page(space, plate_type);
    _space_type = space;
    _plate_type = plate_type;
}

} // namespace
