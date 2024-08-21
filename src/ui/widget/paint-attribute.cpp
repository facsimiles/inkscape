// SPDX-License-Identifier: GPL-2.0-or-later

#include "paint-attribute.h"
#include <glibmm/ustring.h>
#include <gtkmm/enums.h>

#include "stroke-style.h"
#include "object/sp-gradient.h"
#include "object/sp-linear-gradient.h"
#include "object/sp-paint-server.h"
#include "object/sp-pattern.h"
#include "object/sp-radial-gradient.h"
#include "style.h"
#include "object/sp-stop.h"
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

    // refresh paint popup before opening it; it is not kept up-to-date
    _fill._popover.signal_show().connect([this]() {
        set_paint(_current_object, true);
    }, false);
    _stroke._popover.signal_show().connect([this]() {
        set_paint(_current_object, false);
    }, false);
}

void PaintAttribute::PaintStrip::hide() {
    _paint_btn.set_visible(false);
    _alpha.set_visible(false);
    _define.set_visible();
    _clear.set_visible(false);
}

void PaintAttribute::PaintStrip::show() {
    _paint_btn.set_visible();
    _alpha.set_visible();
    _define.set_visible(false);
    _clear.set_visible();
}

PaintAttribute::PaintStrip::PaintStrip(const Glib::ustring& title, bool fill) :
  _label(title)
  // _paint(fill ? FILL : STROKE, true)
{
    _paint_btn.set_direction(Gtk::ArrowType::DOWN);
    _paint_btn.set_always_show_arrow();
    _paint_btn.set_popover(_popover);
    _popover.set_child(*_switch);

    _color_preview.setStyle(ColorPreview::Simple);
    _color_preview.set_frame(true);
    _color_preview.set_border_radius(0);
    _color_preview.set_size_request(16, 16);
    _color_preview.set_checkerboard_tile_size(4);
    _color_preview.set_margin_end(4);
    _color_preview.set_margin_start(1);
    _color_preview.set_halign(Gtk::Align::START);
    _color_preview.set_valign(Gtk::Align::CENTER);
    _paint_type.set_hexpand();
    _paint_type.set_xalign(0.5f);
    _paint_box.append(_color_preview);
    _paint_box.append(_paint_icon);
    _paint_box.append(_paint_type);
    _paint_type.set_text("Gradient");
    _paint_btn.set_child(_paint_box);

    _label.set_halign(Gtk::Align::START);

    auto alpha_adj = Gtk::Adjustment::create(0.0, 0, 100, 1.0, 5.0, 0.0);
    _alpha.set_adjustment(alpha_adj);
    _alpha.set_digits(0);
    _alpha.set_label(C_("Alpha transparency", "A"));
    _alpha.set_suffix(C_("Alpha percent sign", "%"), false);
    _alpha.set_halign(Gtk::Align::START);

    _define.set_image_from_icon_name("plus");
    _define.set_has_frame(false);

    _clear.set_image_from_icon_name("minus");
    _clear.set_has_frame(false);
    _clear.set_visible(false);

    _box.append(_clear);
    _box.append(_define);

    // TEMP code =============================
    // _switch->signal_color_changed().connect([this](auto& color) {
    // });
}

void PaintAttribute::insert_widgets(InkPropertyGrid& grid, int row) {
    _markers.add_css_class("border-box");
    _markers.set_overflow(Gtk::Overflow::HIDDEN);
    _markers.set_spacing(0);
    _markers.set_halign(Gtk::Align::FILL);
    auto scale = 0.6;
    _marker_start.preview_scale(scale);
    _marker_mid.preview_scale(scale);
    _marker_end.preview_scale(scale);
    _markers.append(_marker_start);
    _markers.append(*Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::VERTICAL));
    _markers.append(_marker_mid);
    _markers.append(*Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::VERTICAL));
    _markers.append(_marker_end);

    _size_group->add_widget(_fill._alpha);
    _size_group->add_widget(_stroke._alpha);
    _size_group->add_widget(_unit_selector);

    //TODO: unit-specific adj
    auto adj = Gtk::Adjustment::create(0.0, 0, 1e6, 0.1, 1.0, 0.0);
    _stroke_width.set_adjustment(adj);
    _stroke_width.set_digits(3);
    _stroke_width.set_label(C_("Stroke width", "W"));
    _unit_selector.set_halign(Gtk::Align::START);
    _unit_selector.setUnitType(UNIT_TYPE_LINEAR);
    _unit_selector.addUnit(*UnitTable::get().getUnit("%"));
    _unit_selector.append("hairline", _("Hairline"));
    _stroke_box.set_spacing(1);
    _stroke_box.append(_unit_selector);
    _stroke_box.append(_stroke_presets);
    _stroke_presets.set_halign(Gtk::Align::START);
    _stroke_box.set_halign(Gtk::Align::START);
    //TODO:
    // _stroke_width.set_unit_menu();
    // _old_unit = unitSelector->getUnit();
    // if (desktop) {
    //     unitSelector->setUnit(desktop->getNamedView()->display_units->abbr);
    //     _old_unit = desktop->getNamedView()->display_units;
    // }
    // widthSpin->setUnitMenu(unitSelector);
    // unitSelector->signal_changed().connect(sigc::mem_fun(*this, &StrokeStyle::unitChangedCB));
    // unitSelector->set_visible(true);
    _stroke_presets.set_has_frame(false);
    _stroke_presets.set_icon_name("gear");
    _stroke_presets.set_always_show_arrow(false);
    _stroke_presets.set_popover(_stroke_options);

    grid.add_property(&_fill._label, nullptr, &_fill._paint_btn, &_fill._alpha, &_fill._box);
    _stroke_widgets.add(grid.add_gap());
    grid.add_property(&_stroke._label, nullptr, &_stroke._paint_btn, &_stroke._alpha, &_stroke._box);
    _stroke_widgets.add(grid.add_property(nullptr, nullptr, &_stroke_width, &_stroke_box, nullptr));
    _stroke_widgets.add(grid.add_property(nullptr, nullptr, &_dash_selector, &_markers, nullptr));
    _stroke_widgets.add(grid.add_gap());

    _fill._define.set_tooltip_text(_("Add fill"));
    _fill._clear.set_tooltip_text(_("No fill"));
    _stroke._define.set_tooltip_text(_("Add stroke"));
    _stroke._clear.set_tooltip_text(_("No stroke"));
}

void PaintAttribute::set_document(SPDocument* document) {
    for (auto combo : {&_marker_start, &_marker_mid, &_marker_end}) {
        combo->setDocument(document);
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
    auto mode = get_mode_from_paint(paint);
    auto& stripe = fill ? _fill : _stroke;
    stripe._switch->set_mode(mode);
    if (paint.isColor()) {
        auto color = paint.getColor();
        color.setOpacity(opacity);
        stripe._switch->set_color(color);
    }
    init_popup(paint, opacity, mode, fill);
}

// set correct icon for current fill/stroke type
void PaintAttribute::set_preview(const SPIPaint& paint, double paint_opacity, PaintMode mode, bool fill) {
    auto& stripe = fill ? _fill : _stroke;
    if (mode == PaintMode::None) {
        stripe.hide();
        if (!fill) {
            show_stroke(false);
        }
        return;
    }

    stripe._paint_type.set_text(get_paint_mode_name(mode));

    if (mode == PaintMode::Solid || mode == PaintMode::Swatch) {
        stripe._alpha.set_value(paint_opacity * 100);
        if (mode == PaintMode::Solid) {
            auto color = paint.getColor();
            color.addOpacity(paint_opacity);
            stripe._color_preview.setRgba32(color.toRGBA());
            stripe._color_preview.setIndicator(ColorPreview::None);
        }
        else {
            // swatch
            stripe._color_preview.setIndicator(ColorPreview::Swatch);
            auto server = paint.href->getObject();
            auto swatch = cast<SPGradient>(server);
            assert(swatch);
            auto vect = swatch->getVector();
            auto color = paint.getColor();
            if (auto stop = vect->getFirstStop()) {
                // swatch color is in a first (and only) stop
                color = stop->getColor();
            }
            color.addOpacity(paint_opacity);
            stripe._color_preview.setRgba32(color.toRGBA());
        }
        stripe._color_preview.set_visible();
        stripe._paint_icon.set_visible(false);
        stripe.show();
    }
    else {
        auto icon = get_paint_mode_icon(mode);
        stripe._color_preview.set_visible(false);
        stripe._paint_icon.set_from_icon_name(icon);
        stripe._paint_icon.set_visible();
        stripe.show();
        stripe._alpha.set_visible(false);
    }

    show_stroke(!fill);
}

void PaintAttribute::init_popup(const SPIPaint& paint, double paint_opacity, PaintMode mode, bool fill) {
    auto& stripe = fill ? _fill : _stroke;
    stripe._switch->update_from_paint(paint);
}

void PaintAttribute::update_markers(SPIString* markers[], SPObject* object) {
    for (auto combo : {&_marker_start, &_marker_mid, &_marker_end}) {
        if (combo->in_update()) continue;

        SPObject* marker = nullptr;
        if (auto value = markers[combo->get_loc()]->value()) {
            marker = getMarkerObj(value, object->document);
        }
        combo->setDocument(object->document);
        combo->set_current(marker);
    }
}

void PaintAttribute::show_stroke(bool show) {
    _stroke_widgets.set_visible(show);
}

void PaintAttribute::update_stroke(SPStyle* style) {
    auto unit = _unit_selector.getUnit();

    if (style->stroke_extensions.hairline) {
        _stroke_width.set_sensitive(false);
        _stroke_width.set_value(1);
    }
    else if (unit->type == UNIT_TYPE_LINEAR) {
        double width = Quantity::convert(style->stroke_width.computed, "px", unit);
        _stroke_width.set_value(width);
        _stroke_width.set_sensitive();
    }
    else {
        _stroke_width.set_value(100);
        _stroke_width.set_sensitive();
    }

    double offset = 0;
    auto vec = getDashFromStyle(style, offset);
    _dash_selector.set_dash_pattern(vec, offset);
}

void PaintAttribute::update_from_object(SPObject* object) {
    _current_object = object;

    if (!object || !object->style) {
        // hide
        _fill.hide();
        _stroke.hide();
        //todo: reset document in marker combo?
    }
    else {
        auto& fill_paint = *object->style->getFillOrStroke(true);
        auto fill_mode = get_mode_from_paint(fill_paint);
        set_preview(fill_paint, object->style->fill_opacity, fill_mode, true);

        auto& stroke_paint = *object->style->getFillOrStroke(false);
        auto stroke_mode = get_mode_from_paint(stroke_paint);
        set_preview(stroke_paint, object->style->stroke_opacity, stroke_mode, false);
        update_stroke(object->style);
        update_markers(object->style->marker_ptrs, object);
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
