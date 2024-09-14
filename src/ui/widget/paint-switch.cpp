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
#include "mesh-editor.h"
#include "pattern-manager.h"
#include "widget-group.h"
#include "ui/widget/swatch-editor.h"

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

namespace {

const struct Paint { PaintMode mode; const char* icon; const char* name; const char* tip; } paint_modes[] = {
    {PaintMode::Solid,       "paint-solid",           C_("Paint type", "Flat"),     _("Flat color")},
    {PaintMode::Gradient,    "paint-gradient-linear", C_("Paint type", "Gradient"), _("Linear gradient fill")},
#ifdef WITH_MESH
    {PaintMode::Mesh,        "paint-gradient-mesh",   C_("Paint type", "Mesh"),     _("Mesh fill")},
#endif
    {PaintMode::Pattern,     "paint-pattern",         C_("Paint type", "Pattern"),  _("Pattern fill")},
    {PaintMode::Swatch,      "paint-swatch",          C_("Paint type", "Swatch"),   _("Swatch color")},
    {PaintMode::NotSet,      "paint-unknown",         C_("Paint type", "Unset"),    _("Inherited")},
};

class FlatColorEditor : public Gtk::Box {
    const char* _prefs = "/color-editor";
    std::unique_ptr<ColorPickerPanel> _picker;
public:
    FlatColorEditor(Space::Type space, std::shared_ptr<Colors::ColorSet> colors) :
        _picker(ColorPickerPanel::create(space, get_plate_type_preference(_prefs, ColorPickerPanel::Rect), colors)) {
        append(*_picker);
    }

    void set_color_picker_plate(ColorPickerPanel::PlateType type) {
        _picker->set_plate_type(type);
    }
    ColorPickerPanel::PlateType get_color_picker_plate() const {
        return _picker->get_plate_type();
    }
};

} // namespace

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

    void set_desktop(SPDesktop* desktop) override {
        _swatch.set_desktop(desktop);
    }
    void set_document(SPDocument* document) override {
        _document = document;
        _mesh.set_document(document);
        _swatch.set_document(document);
    }
    // called from the outside to update UI
    void set_mode(PaintMode mode) override;
    // internal handler for buttons switching paint mode
    void switch_paint_mode(PaintMode mode);
    void set_color(const Colors::Color& color) override;
    sigc::signal<void (const Colors::Color&)> get_flat_color_changed() override;
    sigc::signal<void (PaintMode)> get_signal_mode_changed() override;
    void update_from_paint(const SPIPaint& paint) override;
    void _set_mode(PaintMode mode);
    sigc::signal<void (SPGradient* gradient, SPGradientType type)> get_gradient_changed() override {
        return _signal_gradient_changed;
    }
    sigc::signal<void (SPGradient* mesh)> get_mesh_changed() override {
        return _signal_mesh_changed;
    }
    sigc::signal<void (SPGradient* swatch, EditOperation, SPGradient*, std::optional<Color>, Glib::ustring)> get_swatch_changed() override {
        return _signal_swatch_changed;
    }
    sigc::signal<void (SPPattern* pattern, std::optional<Color> color, const Glib::ustring& label,
        const Geom::Affine& transform, const Geom::Point& offset, bool uniform_scale, const Geom::Scale& gap)> get_pattern_changed() override {
        return _signal_pattern_changed;
    }
    void fire_flat_color_changed() {
        if (_update.pending()) return;

        _signal_color_changed.emit(_color->getAverage());
    }
    void fire_pattern_changed() {
        if (_update.pending()) return;

        auto scoped(_update.block());
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

        auto scoped(_update.block());
        auto vector = gradient ? gradient->getVector() : nullptr;
        _signal_gradient_changed.emit(vector, _gradient.get_type());
    }
    void fire_swatch_changed(SPGradient* swatch, EditOperation action, SPGradient* replacement, std::optional<Color> color, Glib::ustring label) {
        if (_update.pending()) return;

        auto scoped(_update.block());
        _signal_swatch_changed.emit(swatch, action, replacement, color, label);
    }
    void fire_mesh_changed(SPGradient* mesh) {
        if (_update.pending()) return;

        auto scoped(_update.block());
        _signal_mesh_changed.emit(mesh);
    }

    // set current page color plate type - circle, rect or none
    void set_plate_type(ColorPickerPanel::PlateType type);
    std::optional<ColorPickerPanel::PlateType> get_plate_type(Gtk::Widget* page) const;
    SPDocument* _document = nullptr;
    std::shared_ptr<Colors::ColorSet> _color = std::make_shared<Colors::ColorSet>();
    Gtk::Stack _stack;
    std::map<PaintMode, Gtk::Widget*> _pages;
    std::map<PaintMode, Gtk::ToggleButton*> _mode_buttons;
    std::map<ColorPickerPanel::PlateType, Gtk::ToggleButton*> _plate_buttons;
    PaintMode _mode = PaintMode::None;
    sigc::signal<void (const Colors::Color&)> _signal_color_changed;
    sigc::signal<void (PaintMode)> _signal_mode_changed;
    sigc::signal<void (SPGradient* gradient, SPGradientType type)> _signal_gradient_changed;
    sigc::signal<void (SPGradient* mesh)> _signal_mesh_changed;
    sigc::signal<void (SPGradient* swatch, EditOperation, SPGradient*, std::optional<Color>, Glib::ustring)> _signal_swatch_changed;
    sigc::signal<void (SPPattern*, std::optional<Color>, const Glib::ustring&, const Geom::Affine&, const Geom::Point&,
                       bool, const Geom::Scale&)> _signal_pattern_changed;
    FlatColorEditor _flat_color{tt, _color};
    GradientEditor _gradient{"/gradient-edit", tt};
    PatternEditor _pattern{"/pattern-edit", PatternManager::get()};
    SwatchEditor _swatch{tt, "/swatch-edit"};
    MeshEditor _mesh;
    Gtk::Box _unset = Gtk::Box{Gtk::Orientation::VERTICAL};
    OperationBlocker _update;
    Gtk::ToggleButton _group;
    Gtk::ToggleButton _mode_group;
    WidgetGroup _plate_type;
};

PaintSwitchImpl::PaintSwitchImpl() {
    _color->set(Color(0x000000ff));
    // _flat_color = ColorPickerPanel::create(tt, pt, _color);
    auto header = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    header->set_margin_top(1);
    header->set_margin_bottom(5);
    header->set_halign(Gtk::Align::FILL);
    auto types = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    types->set_spacing(0);
    types->set_hexpand();

    // add buttons switching paint mode
    for (auto i : paint_modes) {
        auto btn = Gtk::make_managed<Gtk::ToggleButton>();
        btn->set_icon_name(i.icon);
        btn->set_has_frame(false);
        btn->set_tooltip_text(i.tip);
        btn->set_group(_mode_group);
        auto mode = i.mode;
        btn->signal_toggled().connect([=,this](){
            if (btn->get_active() && !_update.pending()) {
                switch_paint_mode(mode);
            }
        });
        types->append(*btn);
        _mode_buttons[i.mode] = btn;
    }

    // add button altering color picker: rect preview, color wheel, sliders only
    auto pickers = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL);
    pickers->set_spacing(0);
    pickers->set_halign(Gtk::Align::END);
    for (auto [icon, type] : { std::tuple
        {"color-picker-rect", ColorPickerPanel::PlateType::Rect},
        {"color-picker-circle", ColorPickerPanel::PlateType::Circle},
        {"color-picker-input", ColorPickerPanel::PlateType::None}}) {

        auto btn = Gtk::make_managed<Gtk::ToggleButton>();
        btn->set_has_frame(false);
        btn->set_icon_name(icon);
        btn->set_group(_group);
        auto name = type;
        btn->signal_toggled().connect([this, name]() { set_plate_type(name); });
        pickers->append(*btn);
        _plate_type.add(btn);
        _plate_buttons[type] = btn;
    }
    header->append(*types);
    header->append(*pickers);

    _mesh.signal_changed().connect([this](auto mesh) { fire_mesh_changed(mesh); });

    _swatch.signal_changed().connect([this](auto swatch, auto operation, auto replacement) {
        fire_swatch_changed(swatch, operation, replacement, {}, {});
    });
    _swatch.signal_color_changed().connect([this](auto swatch, auto& color) {
        fire_swatch_changed(swatch, EditOperation::Change, nullptr, color, {});
    });
    _swatch.signal_label_changed().connect([this](auto swatch, auto& label) {
        fire_swatch_changed(swatch, EditOperation::Rename, nullptr, {}, label);
    });

    //TODO: improve: Gtk::SizeGroup?
    _gradient.set_spinner_size_pattern(get_color_picker_spinner_pattern());
    _gradient.signal_changed().connect([this](auto gradient) { fire_gradient_changed(gradient, _mode); });
    _gradient.set_margin_top(4);

    append(*header);
    auto separator = Gtk::make_managed<Gtk::Separator>();
    // this is problematic, but it works: extend separator
    separator->set_margin_start(-10);
    separator->set_margin_end(-10);
    append(*separator);
    append(_stack);

    _stack.set_hhomogeneous(); // maintain same width
    _stack.set_vhomogeneous(false); // but let height vary
    _stack.set_size_request(-1, 120); // min height
    // _stack.set_transition_type(Gtk::StackTransitionType::CROSSFADE);

    auto undef = Gtk::make_managed<Gtk::Label>(_("Paint is undefined."));
    undef->set_halign(Gtk::Align::START);
    _unset.append(*undef);
    _unset.set_margin_top(4);
    auto info = Gtk::make_managed<Gtk::Label>();
    info->set_markup("<i>" + Glib::Markup::escape_text(_("Paint is not set and can be inherited.")) + "</i>");
    info->set_opacity(0.6);
    info->set_margin_top(20);
    info->set_margin_bottom(20);
    _unset.append(*info);

    // force hight to reveal list of patterns
    _pattern.set_size_request(-1, 440);
    _pattern.signal_changed().connect([this]() { fire_pattern_changed(); });
    _pattern.signal_color_changed().connect([this](auto) { fire_pattern_changed(); });
    _pattern.set_margin_top(4);

    _set_mode(PaintMode::None);

    _pages[PaintMode::Solid]    = &_flat_color;
    _pages[PaintMode::Swatch]   = &_swatch;
    _pages[PaintMode::Gradient] = &_gradient;
    _pages[PaintMode::Pattern]  = &_pattern;
    _pages[PaintMode::Mesh]     = &_mesh;
    _pages[PaintMode::NotSet]   = &_unset;
    for (auto [mode, child] : _pages) {
        if (child) _stack.add(*child);
    }

    _color->signal_changed.connect([this]() {
        fire_flat_color_changed();
    });
}

void PaintSwitchImpl::switch_paint_mode(PaintMode mode) {
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
            fire_gradient_changed(nullptr, mode);
            break;
        case PaintMode::Mesh:
            fire_mesh_changed(nullptr);
            break;
        case PaintMode::Swatch:
        //todo: verify: .getGradientSelector()->getVector();
            fire_swatch_changed(_swatch.get_selected_vector(), EditOperation::New, nullptr, {}, {});
            break;
        case PaintMode::NotSet:
            break;
        default:
            assert(false);
            break;
    }
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
    // auto scoped(_update.block());
    _mode = mode;
    bool has_color_picker = false;

    // show corresponding editor page
    if (auto it = _pages.find(mode); it != end(_pages)) {
        _stack.set_visible_child(*it->second);
        // sync plate type buttons with current page
        auto type = get_plate_type(it->second);
        if (type.has_value()) {
            has_color_picker = true;
            if (auto btn = _plate_buttons[*type]) {
                btn->set_active();
            }
        }
    }
    if (auto mode_btn = _mode_buttons[mode]) {
        mode_btn->set_active();
    }
    // for (auto& kv : _mode_buttons) {
        // kv.second->set_active(mode == kv.first);
    // }
    // color picker available?
    // bool has_picker = mode == PaintMode::Solid || mode == PaintMode::Gradient || mode == PaintMode::Swatch;
    _plate_type.set_sensitive(has_color_picker);
    _plate_type.set_visible(has_color_picker);
}

void PaintSwitchImpl::set_plate_type(ColorPickerPanel::PlateType type) {
    if (auto it = _pages.find(_mode); it != end(_pages)) {
        auto page = it->second;

        if (page == &_flat_color) {
            _flat_color.set_color_picker_plate(type);
        }
        else if (page == &_gradient) {
            _gradient.set_color_picker_plate(type);
        }
        else if (page == &_swatch) {
            _swatch.set_color_picker_plate(type);
        }
    }
}

std::optional<ColorPickerPanel::PlateType> PaintSwitchImpl::get_plate_type(Gtk::Widget* page) const {
    if (page == &_flat_color) {
        return _flat_color.get_color_picker_plate();
    }
    else if (page == &_gradient) {
        return _gradient.get_color_picker_plate();
    }
    else if (page == &_swatch) {
        return _swatch.get_color_picker_plate();
    }

    return {};
}

void PaintSwitchImpl::update_from_paint(const SPIPaint& paint) {
    auto scoped(_update.block());

    auto server = paint.isPaintserver() ? paint.href->getObject() : nullptr;
    if (server) {
        if (is<SPGradient>(server) && cast<SPGradient>(server)->getVector()->isSwatch()) {
            auto vector = cast<SPGradient>(server)->getVector();
            _swatch.select_vector(vector);// .setVector(vector ? vector->document : nullptr, vector);
        }
        else if (is<SPLinearGradient>(server) || is<SPRadialGradient>(server)) {
            auto gradient = cast<SPGradient>(server);
            auto vector = gradient->getVector();
            _gradient.setMode(is<SPLinearGradient>(gradient) ? GradientSelector::MODE_LINEAR : GradientSelector::MODE_RADIAL);
            _gradient.setGradient(gradient);
            _gradient.setVector(vector ? vector->document : nullptr, vector);
            auto stop = cast<SPStop>(const_cast<SPIPaint&>(paint).getTag());
            _gradient.selectStop(stop);
            if (vector) {
                _gradient.setUnits(vector->getUnits());
                _gradient.setSpread(vector->getSpread());
            }
        }
#ifdef WITH_MESH
        else if (is<SPMeshGradient>(server)) {
            //todo
            auto array = cast<SPGradient>(server)->getArray();
            _mesh.select_mesh(array);
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
