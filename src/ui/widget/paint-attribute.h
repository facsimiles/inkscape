// SPDX-License-Identifier: GPL-2.0-or-later

// Simple paint selector widget

#ifndef PAINT_ATTRIBUTE_H
#define PAINT_ATTRIBUTE_H

#include <glib/gi18n.h>
#include <glibmm/ustring.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <gtkmm/menubutton.h>

#include "color-preview.h"
#include "combo-enums.h"
#include "dash-selector.h"
#include "ink-property-grid.h"
#include "ink-spin-button.h"
#include "marker-combo-box.h"
#include "paint-switch.h"
#include "stroke-options.h"
#include "style-internal.h"
#include "unit-menu.h"
#include "widget-group.h"

namespace Inkscape::UI::Widget {

class PaintAttribute {
public:
    PaintAttribute();

    void insert_widgets(InkPropertyGrid& grid, int row);
    void set_document(SPDocument* document);
    void set_desktop(SPDesktop* desktop);
    // update UI from passed object style
    void update_from_object(SPObject* object);

private:
    void set_paint(const SPIPaint& paint, double opacity, bool fill);
    void set_paint(const SPObject* object, bool fill);
    // set icon representing current fill/stroke type
    void set_preview(const SPIPaint& paint, double paint_opacity, PaintMode mode, bool fill);
    // init fill/stroke popup (before it gets opened)
    void init_popup(const SPIPaint& paint, double paint_opacity, PaintMode mode, bool fill);
    //
    void update_markers(SPIString* markers[], SPObject* object);
    // show/hide stroke widgets
    void show_stroke(bool show);
    void update_stroke(SPStyle* style);
    // true if attributes can be modified now, or false while update is pending
    bool can_update() const;

    struct PaintStrip {
        PaintStrip(const Glib::ustring& title, bool fill);

        void show();
        void hide();
        bool can_update() const;
        Gtk::MenuButton _paint_btn;
        Gtk::Popover _popover;
        std::unique_ptr<PaintSwitch> _switch = PaintSwitch::create();
        ColorPreview _color_preview = ColorPreview(0);
        Gtk::Image _paint_icon;
        Gtk::Label _paint_type;
        Gtk::Box _paint_box;
        Gtk::Label _label;
        InkSpinButton _alpha;
        Gtk::Box _box;
        Gtk::Button _define;
        Gtk::Button _clear;
        SPItem* _current_item = nullptr;
        SPDesktop* _desktop = nullptr;
        OperationBlocker* _update = nullptr;
    };
    PaintStrip _fill = PaintStrip(_("Fill"), true);
    PaintStrip _stroke= PaintStrip(_("Stroke"), false);
    Gtk::Box _markers;
    MarkerComboBox _marker_start = MarkerComboBox("marker-start", SP_MARKER_LOC_START);
    MarkerComboBox _marker_mid =   MarkerComboBox("marker-mid", SP_MARKER_LOC_MID);
    MarkerComboBox _marker_end =   MarkerComboBox("marker-end", SP_MARKER_LOC_END);
    Gtk::Box _stroke_box;
    DashSelector _dash_selector = DashSelector(true);
    Gtk::MenuButton _stroke_style;
    Gtk::MenuButton _stroke_presets;
    InkSpinButton _stroke_width;
    UnitMenu _unit_selector;
    Gtk::Popover _stroke_popup;
    StrokeOptions _stroke_options;
    InkSpinButton _opacity;
    Gtk::Button _reset_opacity;
    Gtk::Entry _filter_primitive;
    InkSpinButton _blur;
    Gtk::Button _clear_blur;
    Gtk::Button _edit_filter;
    Gtk::Box _filter_buttons;
    WidgetGroup _filter_widgets;
    ComboBoxEnum<SPBlendMode> _blend;
    Gtk::Button _reset_blend;
    SPItem* _current_item = nullptr;
    Glib::RefPtr<Gtk::SizeGroup> _size_group = Gtk::SizeGroup::create(Gtk::SizeGroup::Mode::HORIZONTAL);
    WidgetGroup _stroke_widgets;
    OperationBlocker _update;
    SPDesktop* _desktop = nullptr;
    const Unit* _current_unit = nullptr;
};

} // namespace

#endif
