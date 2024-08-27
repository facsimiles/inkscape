// SPDX-License-Identifier: GPL-2.0-or-later

#include "paint-switch.h"
#include <glib/gi18n.h>
#include <glibmm/markup.h>
#include <gtkmm/box.h>
#include <gtkmm/enums.h>
#include <gtkmm/label.h>
#include <gtkmm/object.h>
#include <gtkmm/separator.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/widget.h>
#include <memory>
#include <sigc++/connection.h>

#include "color-picker-panel.h"
#include "colors/color-set.h"
// #include "helper/auto-connection.h"
#include "style-internal.h"
#include "ui/operation-blocker.h"
#include "ui/widget/gradient-editor.h"
#include "ui/widget/gradient-selector.h"
#include "ui/widget/pattern-editor.h"
#include "object/sp-hatch.h"
#include "object/sp-linear-gradient.h"
#include "object/sp-mesh-gradient.h"
#include "object/sp-pattern.h"
#include "object/sp-radial-gradient.h"
#include "document.h"
#include "pattern-manager.h"
#include "widget-group.h"
#include "ui/widget/swatch-selector.h"

namespace Inkscape::UI::Widget {

PaintMode get_mode_from_paint(const SPIPaint& paint) {
    if (!paint.set) {
        return PaintMode::NotSet;
    }

    if (auto server = paint.isPaintserver() ? paint.href->getObject() : nullptr) {
        if (is<SPGradient>(server) && cast<SPGradient>(server)->getVector()->isSwatch()) {
            return PaintMode::Swatch;
        }
        else if (is<SPLinearGradient>(server) || is<SPRadialGradient>(server)) {
            return PaintMode::Gradient;
        }
#ifdef WITH_MESH
        else if (is<SPMeshGradient>(server)) {
            return PaintMode::Mesh;
        }
#endif
        else if (is<SPPattern>(server)) {
            return PaintMode::Pattern;
        }
        else if (is<SPHatch>(server)) {
            // mode = MODE_HATCH;
        }
        else {}
    }
    else if (paint.isColor()) {
        return PaintMode::Solid;
    }
    else if (paint.isNone()) {
        return PaintMode::None;
    }

    g_warning("Unexpected paint mode combination\n");
    return PaintMode::NotSet;
}

const static struct Paint { PaintMode mode; const char* icon; const char* name; const char* tip; } paint_modes[] = {
    {PaintMode::Solid,       "paint-solid",           C_("Paint type", "Flat"),     _("Flat color")},
    {PaintMode::Gradient,    "paint-gradient-linear", C_("Paint type", "Gradient"), _("Linear gradient fill")},
    {PaintMode::RadGradient, "paint-gradient-radial", C_("Paint type", "Gradient"), _("Radial gradient fill")},
#ifdef WITH_MESH
    {PaintMode::Mesh,        "paint-gradient-mesh",   C_("Paint type", "Mesh"),     _("Mesh fill")},
#endif
    {PaintMode::Pattern,     "paint-pattern",         C_("Paint type", "Pattern"),  _("Pattern fill")},
    {PaintMode::Swatch,      "paint-swatch",          C_("Paint type", "Swatch"),   _("Swatch color")},
    {PaintMode::NotSet,      "paint-unknown",         C_("Paint type", "Unset"),    _("Inherited")},
};

Glib::ustring get_paint_mode_icon(PaintMode mode) {
    for (auto&& p : paint_modes) {
        if (p.mode == mode) return p.icon;
    }
    return {};
}

Glib::ustring get_paint_mode_name(PaintMode mode) {
    for (auto&& p : paint_modes) {
        if (p.mode == mode) return p.name;
    }
    return {};
}

PaintSwitch::PaintSwitch():
    Gtk::Box(Gtk::Orientation::VERTICAL) {

    set_name("PaintSwitch");
}

//TODO: persist
Colors::Space::Type tt = Space::Type::HSL;
ColorPickerPanel::PlateType pt = ColorPickerPanel::Rect;

class PaintSwitchImpl : public PaintSwitch {
public:
    PaintSwitchImpl();

    void set_document(SPDocument* document) override {
        _document = document;
    }
    void set_mode(PaintMode mode) override;
    void set_color(const Colors::Color& color) override;
    sigc::signal<void (const Colors::Color&)> get_flat_color_changed() override;
    sigc::signal<void (PaintMode)> get_signal_mode_changed() override;
    void update_from_paint(const SPIPaint& paint) override;
    void _set_mode(PaintMode mode);
    sigc::signal<void (SPGradient* gradient, SPGradientType type)> get_gradient_changed() override {
        return _signal_gradient_changed;
    }
    sigc::signal<void (SPPattern* pattern, std::optional<Colors::Color> color, const Glib::ustring& label,
        const Geom::Affine& transform, const Geom::Point& offset, bool uniform_scale, const Geom::Scale& gap)> get_pattern_changed() override {
        return _signal_pattern_changed;
    }
    void fire_flat_color_changed() {
        if (_update.pending()) return;

        _signal_color_changed.emit(_color->getAverage());
    }
    void fire_pattern_changed() {
        if (_update.pending()) return;

        SPPattern* pattern = nullptr;
        auto id = _pattern.get_selected_doc_pattern();
        if (!id.empty() && _document) {
            pattern = cast<SPPattern>(_document->getObjectById(id));
        }
        if (!pattern) {
            auto [id, stock_doc] = _pattern.get_selected_stock_pattern();
            if (!id.empty() && stock_doc) {
                id = "urn:inkscape:pattern:" + id;
                pattern = cast<SPPattern>(get_stock_item(id.c_str(), true, stock_doc));
            }
        }
        _signal_pattern_changed.emit(pattern,
            _pattern.get_selected_color(), _pattern.get_label(), _pattern.get_selected_transform(),
            _pattern.get_selected_offset(), _pattern.is_selected_scale_uniform(), _pattern.get_selected_gap());
    }
    void fire_gradient_changed(SPGradient* gradient, PaintMode mode) {
        if (_update.pending()) return;

        SPGradientType type = SP_GRADIENT_TYPE_LINEAR;
        if (mode == PaintMode::RadGradient) {
            //todo: types
            type = SP_GRADIENT_TYPE_RADIAL;
        }
        else if (mode == PaintMode::Mesh) {
            type = SP_GRADIENT_TYPE_MESH;
        }
        _signal_gradient_changed.emit(gradient, type);
    }
    void fire_swatch_changed() {
        if (_update.pending()) return;

        //todo
    }

    SPDocument* _document = nullptr;
    std::shared_ptr<Colors::ColorSet> _color = std::make_shared<Colors::ColorSet>();
    Gtk::Stack _stack;
    std::map<PaintMode, Gtk::Widget*> _pages;
    std::map<PaintMode, Gtk::ToggleButton*> _mode_buttons;
    PaintMode _mode = PaintMode::None;
    sigc::signal<void (const Colors::Color&)> _signal_color_changed;
    sigc::signal<void (PaintMode)> _signal_mode_changed;
    sigc::signal<void (SPGradient* gradient, SPGradientType type)> _signal_gradient_changed;
    sigc::signal<void (SPPattern*, std::optional<Color>, const Glib::ustring&, const Geom::Affine&, const Geom::Point&, bool, const Geom::Scale&)> _signal_pattern_changed;
    std::unique_ptr<ColorPickerPanel> _flat_color;
    GradientEditor _gradient = {"/gradient-edit", tt};
    PatternEditor _pattern = {"/pattern-edit", PatternManager::get()};
    SwatchSelector _swatch;
    Gtk::Box _unset = Gtk::Box{Gtk::Orientation::VERTICAL};
    OperationBlocker _update;
    Gtk::ToggleButton _group;
    Gtk::ToggleButton _mode_group;
    WidgetGroup _plate_type;
};

PaintSwitchImpl::PaintSwitchImpl() {
    _color->set(Color(0x000000ff));
    _flat_color = ColorPickerPanel::create(tt, pt, _color);
    auto header = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    header->set_margin_top(5);
    header->set_margin_bottom(5);
    header->set_halign(Gtk::Align::FILL);
    auto types = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    types->set_spacing(0);
    types->set_hexpand();

    for (auto i : paint_modes) {
        auto btn = Gtk::make_managed<Gtk::ToggleButton>();
        btn->set_icon_name(i.icon);
        btn->set_has_frame(false);
        btn->set_tooltip_text(i.tip);
        btn->set_group(_mode_group);
        auto mode = i.mode;
        btn->signal_toggled().connect([=,this](){
            if (!btn->get_active() || _update.pending()) return;

            // fire mode change
            _signal_mode_changed.emit(mode);

            switch (mode) {
                case PaintMode::None:
                    break;
                case PaintMode::Solid:
                    fire_flat_color_changed();
                    break;
                case PaintMode::Pattern:
                    fire_pattern_changed();
                    break;
                case PaintMode::Gradient:
                case PaintMode::RadGradient:
                case PaintMode::Mesh:
                    fire_gradient_changed(nullptr, mode);
                    break;
                case PaintMode::Swatch:
                    fire_swatch_changed();
                    break;
                case PaintMode::NotSet:
                    break;
                default:
                    assert(false);
                    break;
            }
        });
        types->append(*btn);
        _mode_buttons[i.mode] = btn;
    }

    auto pickers = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    pickers->set_spacing(0);
    pickers->set_halign(Gtk::Align::END);
    for (auto [id, icon, type] : { std::tuple
        {"rect", "color-picker-rect", ColorPickerPanel::PlateType::Rect},
        {"circle", "color-picker-circle", ColorPickerPanel::PlateType::Circle},
        {"input", "color-picker-input", ColorPickerPanel::PlateType::None}}) {

        auto btn = Gtk::make_managed<Gtk::ToggleButton>();
        btn->set_has_frame(false);
        btn->set_icon_name(icon);
        btn->set_group(_group);
        auto name = type;
        btn->signal_toggled().connect([this, name]() {
            _flat_color->set_plate_type(name);
            _gradient.get_color_picker().set_plate_type(name);
            // todo: others if needed
        });
        pickers->append(*btn);
        _plate_type.add(btn);
    }
    header->append(*types);
    header->append(*pickers);

    //TODO: improve
    _gradient.set_spinner_size_pattern("999.");
    _gradient.signal_changed().connect([this](auto gradient) { fire_gradient_changed(gradient, _mode); });

    append(*header);
    auto separator = Gtk::make_managed<Gtk::Separator>();
    separator->set_margin_bottom(4);
    separator->set_margin_start(-10);
    separator->set_margin_end(-10);
    append(*separator);
    append(_stack);

    _stack.set_hhomogeneous();
    _stack.set_vhomogeneous(false);
    _stack.set_size_request(-1, 120); // min height

    auto undef = Gtk::make_managed<Gtk::Label>(_("Paint is undefined."));
    undef->set_halign(Gtk::Align::START);
    _unset.append(*undef);
    auto info = Gtk::make_managed<Gtk::Label>();
    //todo - correct message, if any
    info->set_markup("<i>" + Glib::Markup::escape_text("Paint is not set and can be inherited.") + "</i>");
    info->set_opacity(0.6);
    info->set_margin_top(20);
    info->set_margin_bottom(20);
    _unset.append(*info);

    // force hight to reveal list of patterns
    _pattern.set_size_request(-1, 440);
    _pattern.signal_changed().connect([this]() { fire_pattern_changed(); });

    _set_mode(PaintMode::None);

    _pages[PaintMode::Solid] = _flat_color.get();
    _pages[PaintMode::Swatch] = &_swatch;
    _pages[PaintMode::Gradient] = &_gradient;
    _pages[PaintMode::Pattern] = &_pattern;
    _pages[PaintMode::NotSet] = &_unset;
    for (auto [mode, child] : _pages) {
        if (child) _stack.add(*child);
    }

    _color->signal_changed.connect([this]() {
        fire_flat_color_changed();
    });
}

void PaintSwitchImpl::set_color(const Colors::Color& color) {
    _color->set(color);
}

sigc::signal<void(const Colors::Color&)> PaintSwitchImpl::get_flat_color_changed() {
    return _signal_color_changed;
}

sigc::signal<void (PaintMode)> PaintSwitchImpl::get_signal_mode_changed() {
    return _signal_mode_changed;
}

void PaintSwitchImpl::set_mode(PaintMode mode) {
    if (mode == _mode) return;

    _set_mode(mode);
}

void PaintSwitchImpl::_set_mode(PaintMode mode) {
    auto scoped(_update.block());
    _mode = mode;

    if (auto it = _pages.find(mode); it != end(_pages)) {
        _stack.set_visible_child(*it->second);
    }
    for (auto& kv : _mode_buttons) {
        kv.second->set_active(mode == kv.first);
    }
    // color picker available?
    bool has_picker = mode == PaintMode::Solid || mode == PaintMode::Gradient || mode == PaintMode::Swatch;
    _plate_type.set_sensitive(has_picker);
    _plate_type.set_visible(has_picker);
}

void PaintSwitchImpl::update_from_paint(const SPIPaint& paint) {
    auto scoped(_update.block());
    auto server = paint.isPaintserver() ? paint.href->getObject() : nullptr;
    if (server) {
        if (is<SPGradient>(server) && cast<SPGradient>(server)->getVector()->isSwatch()) {
            auto vector = cast<SPGradient>(server)->getVector();
            // _psel->setSwatch(vector);
        }
        else if (is<SPLinearGradient>(server) || is<SPRadialGradient>(server)) {
            auto vector = cast<SPGradient>(server)->getVector();
            _gradient.setMode(is<SPLinearGradient>(server) ? GradientSelector::MODE_LINEAR : GradientSelector::MODE_RADIAL);
            _gradient.setGradient(vector);
            _gradient.setVector(vector ? vector->document : nullptr, vector);
            auto stop = cast<SPStop>(const_cast<SPIPaint&>(paint).getTag());
            _gradient.selectStop(stop);
            vector->getUnits();
            _gradient.setUnits(vector->getUnits());
            _gradient.setSpread(vector->getSpread());
        }
#ifdef WITH_MESH
        else if (is<SPMeshGradient>(server)) {
            //todo
            // auto array = cast<SPGradient>(server)->getArray();
            // _psel->setGradientMesh(cast<SPMeshGradient>(array));
            // _psel->updateMeshList(cast<SPMeshGradient>(array));
        }
#endif
        else if (is<SPPattern>(server)) {
            _pattern.set_selected(cast<SPPattern>(server));
        }
    }
    else if (paint.isColor()) {
        //
    }
    else {
        //todo
    }
}

std::unique_ptr<PaintSwitch> PaintSwitch::create() {
    return std::make_unique<PaintSwitchImpl>();
}

} // namespace
