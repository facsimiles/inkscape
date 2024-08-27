// SPDX-License-Identifier: GPL-2.0-or-later

// Simple paint selector widget

#ifndef PAINT_ATTRIBUTE_H
#define PAINT_ATTRIBUTE_H

#include <glib/gi18n.h>
#include <glibmm/ustring.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/grid.h>
#include <gtkmm/label.h>
#include <gtkmm/menubutton.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/widget.h>
#include <memory>

#include "ink-property-grid.h"
#include "ink-spin-button.h"
#include "object/sp-marker-loc.h"
#include "object/sp-paint-server.h"
#include "style-internal.h"
#include "unit-menu.h"
#include "color-preview.h"
#include "ui/widget/dash-selector.h"
#include "ui/widget/marker-combo-box.h"
#include "ui/widget/paint-switch.h"
#include "ui/widget/spinbutton.h"
#include "ui/widget/widget-group.h"

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

    struct PaintStrip {
        PaintStrip(const Glib::ustring& title, bool fill);

        void show();
        void hide();
        bool can_update();
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
    Gtk::Popover _stroke_options;
    SPObject* _current_object = nullptr;
    Glib::RefPtr<Gtk::SizeGroup> _size_group = Gtk::SizeGroup::create(Gtk::SizeGroup::Mode::HORIZONTAL);
    WidgetGroup _stroke_widgets;
    OperationBlocker _update;
    SPDesktop* _desktop = nullptr;
};

} // namespace

#endif
