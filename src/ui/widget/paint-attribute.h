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
#include "object/sp-marker-loc.h"
#include "object/sp-paint-server.h"
#include "style-internal.h"
#include "ui/widget/color-picker.h"
#include "ui/widget/dash-selector.h"
#include "ui/widget/marker-combo-box.h"
#include "ui/widget/paint-switch.h"
#include "ui/widget/spinbutton.h"

namespace Inkscape::UI::Widget {

class PaintAttribute {
public:
    PaintAttribute();

    void insert_widgets(Gtk::Grid& grid, int row);

    void update_from_object(SPObject* object);

private:
    void set_paint(const SPIPaint& paint, double opacity, bool fill);
    void set_paint(const SPObject* object, bool fill);
    // set icon representing current fill/stroke type
    void set_preview(const SPIPaint& paint, double paint_opacity, PaintMode mode, bool fill);
    // init fill/stroke popup (before it gets opened)
    void init_popup(const SPIPaint& paint, double paint_opacity, PaintMode mode, bool fill);

    struct PaintStrip {
        PaintStrip(const Glib::ustring& title);

        void show();
        void hide();
        std::unique_ptr<PaintSwitch> _switch = PaintSwitch::create();
        ColorPicker _picker = ColorPicker(*_switch, _("Select paint"));
        Gtk::Label _label;
        Gtk::SpinButton _alpha;
        Gtk::Box _box;
        Gtk::Button _define;
        Gtk::Button _clear;
    };
    PaintStrip _fill = PaintStrip(_("Fill"));
    PaintStrip _stroke= PaintStrip(_("Stroke"));
    Gtk::Box _markers;
    MarkerComboBox _marker_start = MarkerComboBox("marker-start", SP_MARKER_LOC_START);
    MarkerComboBox _marker_mid =   MarkerComboBox("marker-mid", SP_MARKER_LOC_MID);
    MarkerComboBox _marker_end =   MarkerComboBox("marker-end", SP_MARKER_LOC_END);
    // Gtk::MenuButton _dash_btn;
    DashSelector _dash_selector = DashSelector(true);
    Gtk::MenuButton _stroke_style;
    Gtk::MenuButton _stroke_presets;
    SpinButton _stroke_width;
    SPObject* _current_object = nullptr;
};

} // namespace

#endif
