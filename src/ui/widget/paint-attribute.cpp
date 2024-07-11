// SPDX-License-Identifier: GPL-2.0-or-later

#include "paint-attribute.h"
#include <glibmm/ustring.h>
#include <gtkmm/enums.h>
#include "object/sp-gradient.h"
#include "object/sp-linear-gradient.h"
#include "object/sp-paint-server.h"
#include "object/sp-pattern.h"
#include "object/sp-radial-gradient.h"
#include "style.h"
#include "ui/widget/paint-switch.h"

namespace Inkscape::UI::Widget {

PaintAttribute::PaintAttribute() {
    _marker_start.set_flat(true);
    _marker_mid.set_flat(true);
    _marker_end.set_flat(true);

    _fill._switch->signal_color_changed().connect([this](auto& color) {
        // change fill solid color
        if (_current_object) {
            //todo
        }
    });

    _fill._picker.signal_open_popup().connect([this]() {
        set_paint(_current_object, true);
    });
    _stroke._picker.signal_open_popup().connect([this]() {
        set_paint(_current_object, false);
    });
}

void PaintAttribute::PaintStrip::hide() {
    // _switch = PaintSwitch::create();
    _picker.set_visible(false);
    _alpha.set_visible(false);
    _define.set_visible();
    _clear.set_visible(false);
}

void PaintAttribute::PaintStrip::show() {
    // _switch = PaintSwitch::create();
    _picker.set_visible();
    _alpha.set_visible();
    _define.set_visible(false);
    _clear.set_visible();
}

PaintAttribute::PaintStrip::PaintStrip(const Glib::ustring& title) :
  _label(title)
{
    _picker.set_valign(Gtk::Align::CENTER);

    // set_spacing(4);
    // append(_picker);
    // _label.set_hexpand();
    // _label.set_xalign(0);
    // _label.set_max_width_chars(10);
    // append(_label);
    _label.set_halign(Gtk::Align::START);

    _alpha.set_max_width_chars(5);
    _alpha.set_digits(0);
    _alpha.set_range(0, 100);
    // append(_alpha);

    _define.set_image_from_icon_name("plus");
    _define.set_has_frame(false);
    // _define.set_hexpand();
    // _define.set_halign(Gtk::Align::END);
    // append(_define);

    _clear.set_image_from_icon_name("minus");
    _clear.set_has_frame(false);
    _clear.set_visible(false);
    // _clear.set_hexpand();
    // _clear.set_halign(Gtk::Align::END);
    // append(_clear);

    _box.append(_clear);
    _box.append(_define);
}

void PaintAttribute::insert_widgets(Gtk::Grid& grid, int row) {
    grid.insert_row(row);
    _markers.set_spacing(4);
    _markers.set_halign(Gtk::Align::CENTER);
    _markers.append(_marker_start);
    _markers.append(_marker_mid);
    _markers.append(_marker_end);
    grid.attach(_markers, 0, row, 2);
    _stroke_style.set_valign(Gtk::Align::START);
    grid.attach(_stroke_style, 3, row);

    grid.insert_row(row);
    grid.attach(_stroke_presets, 0, row);
    grid.attach(_stroke_width, 1, row);
    grid.attach(_dash_selector, 3, row);

    for (auto paint : {&_stroke, &_fill}) {
        grid.insert_row(row);

        grid.attach(paint->_picker, 0, row);
        grid.attach(paint->_label, 1, row);
        grid.attach(paint->_alpha, 3, row);
        grid.attach(paint->_box, 4, row);
    }
}

void PaintAttribute::set_paint(const SPObject* object, bool set_fill) {
    if (object && object->style) {
        if (set_fill) {
            if (auto fill = object->style->getFillOrStroke(true)) {
                set_paint(*fill, object->style->fill_opacity, true);
            }
        }
        else {
            if (auto stroke = object->style->getFillOrStroke(false)) {
                set_paint(*stroke, object->style->stroke_opacity, false);
            }
        }
    }
}

void PaintAttribute::set_paint(const SPIPaint& paint, double opacity, bool fill) {
    //todo
    auto mode = get_mode_from_paint(paint);
    auto& stripe = fill ? _fill : _stroke;
    stripe._switch->set_mode(mode);
    if (paint.isColor()) {
        auto color = paint.getColor();
        color.addOpacity(opacity);
        stripe._picker.setColor(color);
        stripe._switch->set_color(color);
    }
    init_popup(paint, opacity, mode, fill);
}

// set correct icon for current fill/stroke type
void PaintAttribute::set_preview(const SPIPaint& paint, double paint_opacity, PaintMode mode, bool fill) {
    auto& stripe = fill ? _fill : _stroke;
    if (mode == PaintMode::None) {
        stripe.hide();
    }
    else if (mode == PaintMode::Solid) {
        auto color = paint.getColor();
        color.addOpacity(paint_opacity);
        stripe._picker.set_icon("");
        stripe._picker.setColor(color);
        stripe.show();
    }
    else {
        auto icon = get_mode_icon(mode);
        stripe._picker.set_icon(icon);
        stripe.show();
    }
}

void PaintAttribute::init_popup(const SPIPaint& paint, double paint_opacity, PaintMode mode, bool fill) {
    auto& stripe = fill ? _fill : _stroke;
    stripe._switch->update_from_paint(paint);
}

void PaintAttribute::update_from_object(SPObject* object) {
    _current_object = object;

    if (!object || !object->style) {
        // hide
        _fill.hide();
        _stroke.hide();
    }
    else {
        auto& fill_paint = *object->style->getFillOrStroke(true);
        auto fill_mode = get_mode_from_paint(fill_paint);
        set_preview(fill_paint, object->style->fill_opacity, fill_mode, true);

        auto& stroke_paint = *object->style->getFillOrStroke(false);
        auto stroke_mode = get_mode_from_paint(stroke_paint);
        set_preview(stroke_paint, object->style->stroke_opacity, stroke_mode, false);
    }
    // else if (object && object->style) {
    //     if (auto fill = object->style->getFillOrStroke(true)) {
    //         set_preview(*fill, true);
    //     }
    //     if (auto stroke = object->style->getFillOrStroke(false)) {
    //         set_preview(*stroke, false);
    //     }
    // }
}

} // namespace
