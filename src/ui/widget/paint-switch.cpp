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
// #include <memory>

#include "color-picker-panel.h"
#include "ui/widget/gradient-editor.h"
#include "ui/widget/pattern-editor.h"
#include "object/sp-hatch.h"
#include "object/sp-linear-gradient.h"
#include "object/sp-mesh-gradient.h"
#include "object/sp-pattern.h"
#include "object/sp-radial-gradient.h"
#include "document.h"
#include "pattern-manager.h"
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

const static struct Paint { PaintMode mode; const char* icon; const char* tip; } paint_modes[] = {
    // {PaintMode::None, "paint-none", _("No paint")},
    {PaintMode::Solid, "paint-solid", _("Flat color")},
    {PaintMode::Gradient, "paint-gradient-linear", _("Gradient fill")},
    {PaintMode::Pattern, "paint-pattern", _("Patter fill")},
    {PaintMode::Swatch, "paint-swatch", _("Swatch color")},
    {PaintMode::NotSet, "paint-unknown", _("Inherited")},
};

PaintSwitch::PaintSwitch():
    Gtk::Box(Gtk::Orientation::VERTICAL) {

    set_name("PaintSwitch");
}

class PaintSwitchImpl : public PaintSwitch {
public:
    PaintSwitchImpl();

    void set_mode(PaintMode mode) override;
    void set_color(const Colors::Color& color) override;
    sigc::signal<void (const Colors::Color&)> signal_color_changed() override;

    void _set_mode(PaintMode mode);
    Gtk::Stack _stack;
    std::map<PaintMode, Gtk::Widget*> _pages;
    PaintMode _mode = PaintMode::None;
    // sigc::signal<void, PaintMode> _signal_changed;
    sigc::signal<void (const Colors::Color&)> _signal_color_changed;
    // SelectedColor _color = std::make_shared<Colors::ColorSet>();
    // std::unique_ptr<ColorSelector> _flat_color = ColorSelector::create();
    std::unique_ptr<ColorPickerPanel> _flat_color = ColorPickerPanel::create();
    // ColorNotebook _flat_color = {_color};
    GradientEditor _gradient = {"/gradient-edit"}; 
    PatternEditor _pattern = {"/pattern-edit", PatternManager::get()};
    SwatchSelector _swatch;
    Gtk::Box _unset = Gtk::Box{Gtk::Orientation::VERTICAL};
};

PaintSwitchImpl::PaintSwitchImpl() {
    auto header = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    header->set_margin(5);
    header->set_spacing(2);
    header->set_halign(Gtk::Align::CENTER);
    for (auto i : paint_modes) {
        auto btn = Gtk::make_managed<Gtk::ToggleButton>();
        btn->set_image_from_icon_name(i.icon);
        btn->set_has_frame(false);
        btn->set_tooltip_text(i.tip);
        auto mode = i.mode;
        btn->signal_toggled().connect([=,this](){
            // turn off other buttons
            if (btn->get_active()) {
                for (auto child : header->get_children()) {
                    if (auto b = dynamic_cast<Gtk::ToggleButton*>(child)) {
                        if (b != btn) b->set_active(false);
                    }
                }
            }
//test:
if (btn->get_active()) set_mode(mode);
        });
        header->append(*btn);
    }

    append(*header);
    auto separator = Gtk::make_managed<Gtk::Separator>();
    separator->set_margin_bottom(4);
    append(*separator);
    append(_stack);

    _stack.set_hhomogeneous();
    _stack.set_vhomogeneous(false);
    _stack.set_size_request(-1, 180); // min height

    auto undef = Gtk::make_managed<Gtk::Label>(_("Paint is undefined."));
    undef->set_halign(Gtk::Align::START);
    _unset.append(*undef);
    auto info = Gtk::make_managed<Gtk::Label>();
    //todo - correct message, if any
    // info->set_markup("<i>" + Glib::Markup::escape_text("Paint is not set and can be inherited.") + "</i>");
    info->set_opacity(0.6);
    info->set_margin_top(20);
    info->set_margin_bottom(20);
    _unset.append(*info);

    _set_mode(PaintMode::None);

    _pages[PaintMode::Solid] = _flat_color.get();
    _pages[PaintMode::Swatch] = &_swatch;
    _pages[PaintMode::Gradient] = &_gradient;
    _pages[PaintMode::Pattern] = &_pattern;
    _pages[PaintMode::NotSet] = &_unset;
    for (auto [mode, child] : _pages) {
        if (child) _stack.add(*child);
    }

    // _flat_color->signal_color_changed().connect([this](auto& c){ _signal_color_changed.emit(c); });
}

void PaintSwitchImpl::set_color(const Colors::Color& color) {
    _flat_color->set_color(color);
}

sigc::signal<void(const Colors::Color&)> PaintSwitchImpl::signal_color_changed() {
    return _signal_color_changed;
}

// void PaintSwitchImpl::update_from_object(SPObject* object) {
    //
// }

void PaintSwitchImpl::set_mode(PaintMode mode) {
    if (mode == _mode) return;

    _set_mode(mode);
}

void PaintSwitchImpl::_set_mode(PaintMode mode) {
    _mode = mode;

    if (auto page = _pages[mode]) {
        _stack.set_visible_child(*page);
    }
}

std::unique_ptr<PaintSwitch> PaintSwitch::create() {
    return std::make_unique<PaintSwitchImpl>();
}

} // namespace
