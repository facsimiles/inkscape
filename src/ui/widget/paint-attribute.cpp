// SPDX-License-Identifier: GPL-2.0-or-later

#include "paint-attribute.h"

#include <numeric>
#include <boost/smart_ptr/intrusive_ptr.hpp>
#include <glibmm/ustring.h>
#include <gtkmm/enums.h>

#include "desktop-style.h"
#include "document-undo.h"
#include "filter-chemistry.h"
#include "filter-effect-chooser.h"
#include "filter-enums.h"
#include "gradient-chemistry.h"
#include "inkscape.h"
#include "object-composite-settings.h"
#include "pattern-manipulation.h"
#include "property-utils.h"
#include "stroke-style.h"
#include "object/sp-gradient.h"
#include "object/sp-radial-gradient.h"
#include "object/sp-paint-server.h"
#include "object/sp-pattern.h"
#include "style.h"
#include "actions/actions-tools.h"
#include "object/sp-namedview.h"
#include "object/sp-stop.h"
#include "svg/css-ostringstream.h"
#include "ui/util.h"
#include "ui/dialog/dialog-container.h"
#include "ui/tools/marker-tool.h"
#include "ui/widget/paint-switch.h"
#include "util/expression-evaluator.h"
#include "xml/sp-css-attr.h"

namespace Inkscape::UI::Widget {

using namespace Inkscape::UI::Utils;

PaintAttribute::PaintAttribute():
    _blend(get_blendmode_combo_converter(), SPAttr::INVALID, false, "BlendMode")
{
    _marker_start.set_flat(true);
    _marker_mid.set_flat(true);
    _marker_end.set_flat(true);

    _fill._update = &_update;
    _stroke._update = &_update;

    // refresh paint popup before opening it; it is not kept up-to-date
    _fill._popover.signal_show().connect([this]() {
        set_paint(_current_item, true);
    }, false);
    _stroke._popover.signal_show().connect([this]() {
        set_paint(_current_item, false);
    }, false);
}

void PaintAttribute::PaintStripe::hide() {
    _paint_btn.set_visible(false);
    _alpha.set_visible(false);
    _define.set_visible();
    _clear.set_visible(false);
}

void PaintAttribute::PaintStripe::show() {
    _paint_btn.set_visible();
    _alpha.set_visible();
    _define.set_visible(false);
    _clear.set_visible();
}

bool PaintAttribute::PaintStripe::can_update() const {
    return _current_item && _update && !_update->pending();
}

namespace {

boost::intrusive_ptr<SPCSSAttr> new_css_attr() {
    return boost::intrusive_ptr(sp_repr_css_attr_new(), false);
}

void set_item_style(SPItem* item, SPCSSAttr* css) {
    item->changeCSS(css, "style");
}

void set_item_style_str(SPItem* item, const char* attr, const char* value) {
    auto css = new_css_attr();
    sp_repr_css_set_property(css.get(), attr, value);
    set_item_style(item, css.get());
}

void set_item_style_dbl(SPItem* item, const char* attr, double value) {
    CSSOStringStream os;
    os << value;
    set_item_style_str(item, attr, os.str().c_str());
}

void set_stroke_width(SPItem* item, double width_typed, bool hairline, const Unit* unit) {
    auto css = new_css_attr();
    if (hairline) {
        // For renderers that don't understand -inkscape-stroke:hairline, fall back to 1px non-scaling
        width_typed = 1;
        sp_repr_css_set_property(css.get(), "vector-effect", "non-scaling-stroke");
        sp_repr_css_set_property(css.get(), "-inkscape-stroke", "hairline");
    }
    else {
        sp_repr_css_unset_property(css.get(), "vector-effect");
        sp_repr_css_unset_property(css.get(), "-inkscape-stroke");
    }

    double width = calc_scale_line_width(width_typed, item, unit);
    sp_repr_css_set_property_double(css.get(), "stroke-width", width);

    if (Preferences::get()->getBool("/options/dash/scale", true)) {
        // This will read the old stroke-width to un-scale the pattern.
        double offset = 0;
        auto dash = getDashFromStyle(item->style, offset);
        set_scaled_dash(css.get(), dash.size(), dash.data(), offset, width);
    }
    set_item_style(item, css.get());
}

void set_item_marker(SPItem* item, int location, const char* attr, const std::string& uri) {
    set_item_style_str(item, attr, uri.c_str());
    //TODO: verify if any of the below lines are needed
    // item->requestModified(SP_OBJECT_MODIFIED_FLAG);
    // item->requestDisplayUpdate(SP_OBJECT_MODIFIED_FLAG | SP_OBJECT_STYLE_MODIFIED_FLAG);
    // needed?
    item->document->ensureUpToDate();
}

void edit_marker(int location, SPDesktop* desktop) {
    if (!desktop) return;

    set_active_tool(desktop, "Marker");
    if (auto marker_tool = dynamic_cast<Tools::MarkerTool*>(desktop->getTool())) {
        marker_tool->editMarkerMode = location;
        marker_tool->selection_changed(desktop->getSelection());
    }
}

std::optional<Colors::Color> get_item_color(SPItem* item, bool fill) {
    if (!item || !item->style) return {};

    auto paint = item->style->getFillOrStroke(fill);
    return paint && paint->isColor() ? std::optional(paint->getColor()) : std::nullopt;
}

void swatch_operation(SPItem* item, SPGradient* vector, SPDesktop* desktop, bool fill, EditOperation operation, SPGradient* replacement, std::optional<Color> color, Glib::ustring label) {
    auto kind = fill ? FILL : STROKE;

    switch (operation) {
    case EditOperation::New:
        // try to find existing swatch with matching color definition:
        if (auto clr = get_item_color(item, fill)) {
            vector = sp_find_matching_swatch(item->document, *clr);
        }
        else {
            // create a new swatch
            vector = nullptr;
        }
        sp_item_apply_gradient(item, vector, desktop, SP_GRADIENT_TYPE_LINEAR, true, kind);
        DocumentUndo::done(item->document, fill ? _("Set swatch on fill") : _("Set swatch on stroke"), "dialog-fill-and-stroke");
        break;
    case EditOperation::Change:
        if (color.has_value()) {
            sp_change_swatch_color(vector, *color);
            DocumentUndo::maybeDone(item->document, "swatch-color", _("Change swatch color"), "dialog-fill-and-stroke");
        }
        else {
            sp_item_apply_gradient(item, vector, desktop, SP_GRADIENT_TYPE_LINEAR, true, kind);
            DocumentUndo::maybeDone(item->document, fill ? "fill-swatch-change" : "stroke-swatch-change", fill ? _("Set swatch on fill") : _("Set swatch on stroke"), "dialog-fill-and-stroke");
        }
        break;
    case EditOperation::Delete:
        sp_delete_item_swatch(item, kind, vector, replacement);
        DocumentUndo::done(item->document, _("Delete swatch"), "dialog-fill-and-stroke");
        break;
    case EditOperation::Rename:
        vector->setLabel(label.c_str());
        DocumentUndo::maybeDone(item->document, _("Rename swatch"), "swatch-rename", "dialog-fill-and-stroke");
        break;
    default:
        break;
    }
}

} // namespace

// size of color preview tiles
constexpr int COLOR_TILE = 16;

PaintAttribute::PaintStripe::PaintStripe(const Glib::ustring& title, bool fill) :
  _label(title)
{
    _paint_btn.set_direction(Gtk::ArrowType::DOWN);
    _paint_btn.set_always_show_arrow();
    _paint_btn.set_popover(_popover);
    _popover.set_child(*_switch);

    _color_preview.setStyle(ColorPreview::Simple);
    _color_preview.set_frame(true);
    _color_preview.set_border_radius(0);
    _color_preview.set_size_request(COLOR_TILE, COLOR_TILE);
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

    SpinPropertyDef properties[] = {
        {&_alpha, { 0, 100, 1, 5, 0, 100 }, C_("Alpha transparency", "A"), fill ? _("Fill opacity") : _("Stroke opacity"), Percent},
    };
    for (auto& def : properties) {
        init_spin_button(def);
    }
    _alpha.set_halign(Gtk::Align::START);

    init_property_button(_define, Add, fill ? _("Add fill") : _("Add stroke"));
    init_property_button(_clear, Remove, fill ? _("No fill") : _("No stroke"));
    _clear.set_visible(false);

    _box.append(_clear);
    _box.append(_define);

    _clear.signal_clicked().connect([this,fill]() {
        if (!can_update()) return;

        //TODO: skip locked items?
        //TODO: skip <use> items?

        set_item_style_str(_current_item, fill ? "fill" : "stroke", "none");

        DocumentUndo::done(_current_item->document, fill ? _("Remove fill") : _("Remove stroke"), "dialog-fill-and-stroke");
    });

    auto set_flat_color = [this,fill](const Color& color) {
        if (!can_update()) return;

        // sp_desktop_set_color(_desktop, _solid_colors->getAverage(), false, kind == FILL);
        auto css = new_css_attr();
        sp_repr_css_set_property_string(css.get(), fill ? "fill" : "stroke", color.toString(false));
        sp_repr_css_set_property_double(css.get(), fill ? "fill-opacity" : "stroke-opacity", color.getOpacity());
        //TODO: skip locked item?
        set_item_style(_current_item, css.get());

        DocumentUndo::maybeDone(_current_item->document, fill ? "change-fill" : "change-stroke",
            fill ? _("Set fill color") : _("Set stroke color"), "dialog-fill-and-stroke");
    };

    _define.signal_clicked().connect([=,this]() {
        if (!can_update()) return;

        // add fill or stroke
        set_flat_color(Color(0x909090ff));
    });

    _switch->get_pattern_changed().connect([this,fill](auto pattern, auto color, auto label, auto transform, auto offset, auto uniform, auto gap) {
        if (!can_update()) return;

        // auto scoped(_update->block());
        auto kind = fill ? FILL : STROKE;
        sp_item_apply_pattern(_current_item, pattern, kind, color, label, transform, offset, uniform, gap);
        DocumentUndo::maybeDone(_current_item->document, fill ? "fill-pattern-change" : "stroke-pattern-change", fill ? _("Set pattern on fill") : _("Set pattern on stroke"), "dialog-fill-and-stroke");
    });

    _switch->get_gradient_changed().connect([this,fill](auto vector, auto gradient_type) {
        if (!can_update()) return;

        auto kind = fill ? FILL : STROKE;
        sp_item_apply_gradient(_current_item, vector, _desktop, gradient_type, false, kind);
        DocumentUndo::maybeDone(_current_item->document, fill ? "fill-gradient-change" : "stroke-gradient-change", fill ? _("Set gradient on fill") : _("Set gradient on stroke"), "dialog-fill-and-stroke");
    });

    _switch->get_mesh_changed().connect([this,fill](auto mesh) {
        if (!can_update()) return;

        auto kind = fill ? FILL : STROKE;
        sp_item_apply_mesh(_current_item, mesh, _current_item->document, kind);
        DocumentUndo::maybeDone(_current_item->document, fill ? "fill-mesh-change" : "stroke-mesh-change", fill ? _("Set mesh on fill") : _("Set mesh on stroke"), "dialog-fill-and-stroke");
    });

    _switch->get_swatch_changed().connect([this,fill](auto vector, auto operation, auto replacement, std::optional<Color> color, auto label) {
        if (!can_update()) return;

        swatch_operation(_current_item, vector, _desktop, fill, operation, replacement, color, label);
    });

    _switch->get_flat_color_changed().connect([=,this](auto& color) {
        set_flat_color(color);
    });

    _switch->get_signal_mode_changed().connect([=,this](auto mode) {
        if (!can_update()) return;

        if (mode == PaintMode::NotSet) {
            // unset
            _current_item->removeAttribute(fill ? "fill" : "stroke");
            auto css = new_css_attr();
            if (fill) {
                sp_repr_css_unset_property(css.get(), "fill");
            }
            else {
                for (auto attr : {
                    "stroke", "stroke-opacity", "stroke-width", "stroke-miterlimit", "stroke-linejoin",
                    "stroke-linecap", "stroke-dashoffset", "stroke-dasharray"}) {
                    sp_repr_css_unset_property(css.get(), attr);
                }
            }
            set_item_style(_current_item, css.get());
            DocumentUndo::done(_current_item->document,  fill ? _("Unset fill") : _("Unset stroke"), "dialog-fill-and-stroke");
        }
    });

    _alpha.signal_value_changed().connect([this,fill](auto alpha) {
        if (!can_update()) return;

        auto css = new_css_attr();
        sp_repr_css_set_property_double(css.get(), fill ? "fill-opacity" : "stroke-opacity", alpha);
        //TODO: skip locked item?
        set_item_style(_current_item, css.get());

        DocumentUndo::maybeDone(_current_item->document, fill ? "undo_fill_alpha" : "undo_stroke_alpha",
            fill ? _("Set fill opacity") : _("Set stroke opacity"), "dialog-fill-and-stroke");
    });
}

void PaintAttribute::insert_widgets(InkPropertyGrid& grid) {
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

    auto set_marker = [this](int location, const char* id, const std::string& uri) {
        if (!can_update()) return;

        set_item_marker(_current_item, location, id, uri);
        DocumentUndo::maybeDone(_current_item->document, "marker-change", _("Set marker"), "dialog-fill-and-stroke");
    };

    for (auto combo : {&_marker_start, &_marker_mid, &_marker_end}) {
        combo->connect_changed([=]() {
            if (!combo->in_update()) {
                set_marker(combo->get_loc(), combo->get_id(), combo->get_active_marker_uri());
            }
        });

        // request to edit current marker on the canvas
        combo->connect_edit([this,combo](){ edit_marker(combo->get_loc(), _desktop); });
    }

    //TODO: unit-specific adj?
    // 4 digits of precision to allow meters to show some values when switching from single pixels
    SpinPropertyDef width_prop = {&_stroke_width, { 0, 1e6, 0.1, 1.0, 4 }, C_("Stroke width", "W"), _("Stroke width") };
    init_spin_button(width_prop);
    _stroke_width.set_evaluator_function([this](auto& text) {
        auto unit = _unit_selector.getUnit();
        auto result = ExpressionEvaluator(text.c_str(), unit).evaluate();
        // check if output dimension corresponds to input unit
        if (result.dimension != (unit->isAbsolute() ? 1 : 0) ) {
            throw EvaluatorException("Input dimensions do not match with parameter dimensions.", "");
        }
        return result.value;
    });
    auto set_stroke = [this](double width) {
        if (!can_update()) return;

        auto scoped(_update.block());
        auto hairline = _unit_selector.get_active_id() == "hairline";
        auto unit = _unit_selector.getUnit();
        set_stroke_width(_current_item, width, hairline, unit);
        DocumentUndo::done(_current_item->document, _("Set stroke width"), "dialog-fill-and-stroke");
    };
    auto set_stroke_unit = [=,this]() {
        if (!can_update()) return;

        auto new_unit = _unit_selector.getUnit();
        if (new_unit == _current_unit) return;

        auto hairline = _unit_selector.get_active_id() == "hairline";
        auto width = _stroke_width.get_value();
        if (hairline) {
            auto scoped(_update.block());
            _current_unit = new_unit;
            set_stroke_width(_current_item, 1, hairline, new_unit);
            DocumentUndo::done(_current_item->document, _("Set stroke width"), "dialog-fill-and-stroke");
        }
        else {
            // if current unit is empty, then it's a hairline, b/c it's not in a unit table
            if (_current_unit->abbr.empty()) {
                _current_unit = UnitTable::get().getUnit("px");
            }
            width = Quantity::convert(width, _current_unit, new_unit);
            _current_unit = new_unit;
            {
                auto scoped(_update.block());
                _stroke_width.set_value(width);
            }
            set_stroke(width);
        }
    };
    auto set_stroke_style = [this](const char* attr, const char* value) {
        if (!can_update()) return;

        auto scoped(_update.block());
        set_item_style_str(_current_item, attr, value);
        DocumentUndo::done(_current_item->document, _("Set stroke style"), "dialog-fill-and-stroke");
    };
    auto set_stroke_miter_limit = [this](double limit) {
        if (!can_update()) return;

        auto scoped(_update.block());
        set_item_style_dbl(_current_item, "stroke-miterlimit", limit);
        DocumentUndo::done(_current_item->document, _("Set stroke miter"), "dialog-fill-and-stroke");
    };
    _stroke_width.signal_value_changed().connect([=,this](auto value) {
        set_stroke(value);
    });
    _unit_selector.set_halign(Gtk::Align::START);
    _unit_selector.setUnitType(UNIT_TYPE_LINEAR);
    _unit_selector.append("hairline", _("Hairline"));
    _unit_selector.signal_changed().connect([=,this]() {
        set_stroke_unit();
    });
    _stroke_box.set_spacing(1);
    _stroke_box.append(_unit_selector);
    _stroke_presets.set_halign(Gtk::Align::START);
    _stroke_presets.set_tooltip_text(_("Stroke options"));
    _stroke_box.set_halign(Gtk::Align::START);
    _stroke_presets.set_has_frame(false);
    _stroke_presets.set_icon_name("gear");
    _stroke_presets.set_always_show_arrow(false);
    _stroke_presets.set_popover(_stroke_popup);
    _stroke_popup.set_child(_stroke_options);
    _stroke_options._join_changed.connect([=](auto style) {
        set_stroke_style("stroke-linejoin", style);
    });
    _stroke_options._cap_changed.connect([=](auto style) {
        set_stroke_style("stroke-linecap", style);
    });
    _stroke_options._order_changed.connect([=](auto style) {
        set_stroke_style("pant-order", style);
    });
    _stroke_options._miter_changed.connect([=](auto value) {
        set_stroke_miter_limit(value);
    });

    grid.add_property(&_fill._label, nullptr, &_fill._paint_btn, &_fill._alpha, &_fill._box);
    _stroke_widgets.add(grid.add_gap());
    grid.add_property(&_stroke._label, nullptr, &_stroke._paint_btn, &_stroke._alpha, &_stroke._box);
    _stroke_widgets.add(grid.add_property(nullptr, nullptr, &_stroke_width, &_stroke_box, &_stroke_presets));
    _stroke_widgets.add(grid.add_property(nullptr, nullptr, &_dash_selector, &_markers, nullptr));
    _stroke_widgets.add(grid.add_gap());

    auto set_dash = [this](bool pattern_edit) {
        if (!can_update()) return;

        auto scoped(_update.block());
        auto item = _current_item;
        auto& dash = pattern_edit ? _dash_selector.get_custom_dash_pattern() : _dash_selector.get_dash_pattern();
        auto offset = _dash_selector.get_offset();
        double scale = item->i2doc_affine().descrim();
        if (Preferences::get()->getBool("/options/dash/scale", true)) {
            scale = item->style->stroke_width.computed * scale;
        }
        auto css = new_css_attr();
        set_scaled_dash(css.get(), dash.size(), dash.data(), offset, scale);
        set_item_style(item, css.get());
    };
    _dash_selector.changed_signal.connect([=](auto change) {
        set_dash(change == DashSelector::Pattern);
    });

    SpinPropertyDef properties[] = {
        {&_opacity, { 0, 100, 1, 5, 1, 100 }, nullptr, _("Object's opacity"), Percent, &_reset_opacity},
        {&_blur,    { 0, 100, 1, 5, 1, 100 }, nullptr, _("Blur filter"), Percent},
    };
    for (auto& def : properties) {
        init_spin_button(def);
    }
    init_property_button(_clear_blur, Reset);
    init_property_button(_edit_filter, Edit, _("Edit filter"));
    _edit_filter.set_visible(false);
    _filter_buttons.append(_clear_blur);
    _filter_buttons.append(_edit_filter);
    _filter_primitive.set_editable(false);
    _filter_primitive.set_can_focus(false);
    _filter_primitive.set_focusable(false);
    _filter_primitive.set_focus_on_click(false);
    _filter_primitive.set_max_width_chars(8);
    init_property_button(_reset_blend, Reset, _("Normal blend mode"));
    grid.add_property(_("Opacity"), nullptr, &_opacity, nullptr, &_reset_opacity);
    grid.add_property(_("Blend mode"), nullptr, &_blend, nullptr, &_reset_blend);
    _filter_widgets = grid.add_property(_("Filter"), nullptr, &_filter_primitive, &_blur, &_filter_buttons);
    grid.add_gap();

    _clear_blur.signal_clicked().connect([this]() {
        if (!can_update()) return;

        auto scoped(_update.block());
        if (remove_filter_gaussian_blur(_current_item)) {
            DocumentUndo::done(_current_item->document, _("Remove filter"), "dialog-fill-and-stroke");
        }
    });
    _edit_filter.signal_clicked().connect([this]() {
        if (!_desktop) return;
        // open filter editor
        if (auto container = _desktop->getContainer()) {
            container->new_dialog("FilterEffects");
        }
    });

    auto set_object_opacity = [this](double opacity) {
        if (!can_update()) return;

        auto item = _current_item;
        auto scoped(_update.block());
        auto css = new_css_attr();
        sp_repr_css_set_property_double(css.get(), "opacity", opacity);
        set_item_style(item, css.get());

        DocumentUndo::done(item->document, _("Set opacity"), "dialog-fill-and-stroke");
    };
    _opacity.signal_value_changed().connect([=,this](auto value){ set_object_opacity(value); });
    _reset_opacity.signal_clicked().connect([=,this](){ set_object_opacity(1.0); });

    auto set_blend_mode = [=,this](SPBlendMode mode) {
        if (!can_update()) return;

        auto scoped(_update.block());
        if (::set_blend_mode(_current_item, mode)) {
            DocumentUndo::done(_current_item->document, _("Set blending mode"), "dialog-fill-and-stroke");
        }
    };
    _blend.signal_changed().connect([=,this]() {
        if (auto data = _blend.get_active_data()) {
            set_blend_mode(data->id);
        }
    });
    _reset_blend.signal_clicked().connect([=,this]() {
        set_blend_mode(SP_CSS_BLEND_NORMAL);
    });
}

void PaintAttribute::set_document(SPDocument* document) {
    for (auto combo : {&_marker_start, &_marker_mid, &_marker_end}) {
        combo->setDocument(document);
    }
    _fill._switch->set_document(document);
    _stroke._switch->set_document(document);
}

void PaintAttribute::set_desktop(SPDesktop* desktop) {
    if (_desktop != desktop && desktop) {
        auto unit = desktop->getNamedView()->display_units;
        if (unit != _unit_selector.getUnit()) {
            auto scoped(_update.block());
            _unit_selector.setUnit(unit->abbr);
        }
        _current_unit = unit;
    }
    _desktop = desktop;
    _fill._switch->set_desktop(desktop);
    _stroke._switch->set_desktop(desktop);
}

void PaintAttribute::set_paint(const SPObject* object, bool set_fill) {
    if (!object || !object->style) return;

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

void PaintAttribute::set_paint(const SPIPaint& paint, double opacity, bool fill) {
    auto scoped(_update.block());

    auto mode = get_mode_from_paint(paint);
    auto& stripe = fill ? _fill : _stroke;
    stripe._switch->set_mode(mode);
    if (paint.isColor()) {
        auto color = paint.getColor();
        color.setOpacity(opacity);
        stripe._switch->set_color(color);
    }
    stripe._switch->update_from_paint(paint);
}

// set correct icon for current fill/stroke type
void PaintAttribute::set_preview(const SPIPaint& paint, double paint_opacity, PaintMode mode, bool fill) {
    auto& stripe = fill ? _fill : _stroke;
    if (mode == PaintMode::None) {
        stripe.hide();
        return;
    }

    stripe._paint_type.set_text(get_paint_mode_name(mode));

    if (mode == PaintMode::Solid || mode == PaintMode::Swatch || mode == PaintMode::Gradient) {
        stripe._alpha.set_value(paint_opacity);
        if (mode == PaintMode::Solid) {
            auto color = paint.getColor();
            color.addOpacity(paint_opacity);
            stripe._color_preview.setRgba32(color.toRGBA());
            stripe._color_preview.setIndicator(ColorPreview::None);
        }
        else if (mode == PaintMode::Swatch) {
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
        else {
            // gradients
            auto server = paint.href->getObject();
            auto pat_t = cast<SPGradient>(server)->create_preview_pattern(COLOR_TILE);
            auto pat = Cairo::RefPtr<Cairo::Pattern>(new Cairo::Pattern(pat_t, true));
            stripe._color_preview.setPattern(pat);
            stripe._color_preview.setIndicator(is<SPRadialGradient>(server) ? ColorPreview::RadialGradient : ColorPreview::LinearGradient);
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
    }
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
    if (style->stroke_extensions.hairline) {
        _stroke_width.set_sensitive(false);
        _stroke_width.set_value(1);
    }
    else {
        auto unit = _unit_selector.getUnit();
        double width = Quantity::convert(style->stroke_width.computed, "px", unit);
        _stroke_width.set_value(width);
        _stroke_width.set_sensitive();
    }

    double offset = 0;
    auto vec = getDashFromStyle(style, offset);
    _dash_selector.set_dash_pattern(vec, offset);
}

bool PaintAttribute::can_update() const {
    return _current_item && _current_item->style && !_update.pending();
}

void PaintAttribute::update_from_object(SPObject* object) {
    // if (_update.pending()) return;

    auto scoped(_update.block());

    _current_item = cast<SPItem>(object);
    _fill._current_item = _current_item;
    _stroke._current_item = _current_item;
    _fill._desktop = _desktop;
    _stroke._desktop = _desktop;

    if (!_current_item || !_current_item->style) {
        // hide
        _fill.hide();
        _stroke.hide();
        //todo: reset document in marker combo?
    }
    else {
        auto& style = object->style;
        auto& fill_paint = *style->getFillOrStroke(true);
        auto fill_mode = get_mode_from_paint(fill_paint);
        set_preview(fill_paint, style->fill_opacity, fill_mode, true);
        if (_fill._popover.is_visible()) {
            set_paint(_current_item, true);
        }

        auto& stroke_paint = *style->getFillOrStroke(false);
        auto stroke_mode = get_mode_from_paint(stroke_paint);
        set_preview(stroke_paint, style->stroke_opacity, stroke_mode, false);
        if (_stroke._popover.is_visible()) {
            set_paint(_current_item, false);
        }
        update_stroke(style);
        update_markers(style->marker_ptrs, object);
        if (stroke_mode != PaintMode::None) {
            _stroke_options.update_widgets(*style);
            show_stroke(true);
        }
        else {
            show_stroke(false);
        }

        double opacity = style->opacity;
        _opacity.set_value(opacity);
        _reset_opacity.set_visible(opacity != 1.0);

        auto blend_mode = style->mix_blend_mode.set ? style->mix_blend_mode.value : SP_CSS_BLEND_NORMAL;
        _blend.set_active_by_id(blend_mode);
        _reset_blend.set_visible(blend_mode != SP_CSS_BLEND_NORMAL);

        auto filters = get_filter_primitive_count(object);
        double blur = 0;
        if (filters == 1) {
            auto primitive = get_first_filter_component(object);
            auto id = FPConverter.get_id_from_key(primitive->getRepr()->name());
            _filter_primitive.set_text(_(FPConverter.get_label(id).c_str()));
            if (id == Filters::NR_FILTER_GAUSSIANBLUR) {
                auto item = cast<SPItem>(object);
                if (auto radius = object_query_blur_filter(item)) {
                    if (auto bbox = item->desktopGeometricBounds()) {
                        double perimeter = bbox->dimensions()[Geom::X] + bbox->dimensions()[Geom::Y];
                        blur = std::sqrt(*radius * BLUR_MULTIPLIER / perimeter);
                    }
                }
            }
            _blur.set_value(blur);
            _blur.set_sensitive(blur > 0);
        }
        else if (filters > 1) {
            _filter_primitive.set_text(_("Compound filter"));
            _blur.set_value(0);
            _blur.set_sensitive(false);
        }
        else {
            _filter_primitive.set_text({});
            _blur.set_value(0);
            _blur.set_sensitive(false);
        }
        _filter_widgets.set_visible(filters > 0);
        _clear_blur.set_visible(blur != 0);
        _edit_filter.set_visible(blur == 0 && filters > 0);
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
