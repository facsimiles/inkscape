// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Combobox for selecting dash patterns - implementation.
 */
/* Author:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Maximilian Albert <maximilian.albert@gmail.com>
 *
 * Copyright (C) 2002 Lauris Kaplinski
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_SP_MARKER_COMBO_BOX_H
#define SEEN_SP_MARKER_COMBO_BOX_H

#include <giomm/liststore.h>
#include <gtkmm/cellrendererpixbuf.h>
#include <gtkmm/treemodel.h>

#include "display/drawing.h"
#include "document.h"
#include "ink-property-grid.h"
#include "ink-spin-button.h"
#include "ui/operation-blocker.h"
#include "ui/widget/widget-vfuncs-class-init.h"

namespace Gtk {
class Builder;
class Button;
class CheckButton;
class FlowBox;
class Image;
class Label;
class MenuButton;
class Picture;
class SpinButton;
class ToggleButton;
} // namespace Gtk

class SPDocument;
class SPMarker;
class SPObject;

namespace Inkscape::UI::Widget {

class Bin;

/**
 * ComboBox-like class for selecting stroke markers.
 */
class MarkerComboBox : public WidgetVfuncsClassInit, public Gtk::MenuButton {
public:
    MarkerComboBox(Glib::ustring id, int loc);

    void setDocument(SPDocument *);

    void set_current(SPObject *marker);
    std::string get_active_marker_uri();
    bool in_update() const { return _update.pending(); };
    const char* get_id() const { return _combo_id.c_str(); };
    int get_loc() const { return _loc; };

    sigc::connection connect_changed(sigc::slot<void ()> slot);
    sigc::connection connect_edit   (sigc::slot<void ()> slot);
    // set a flat look
    void set_flat(bool flat);
    // scale default marker preview size; 1.0 by default (=40x32)
    void preview_scale(double scale);

private:
    struct MarkerItem : Glib::Object {
        Cairo::RefPtr<Cairo::Surface> pix;
        SPDocument* source = nullptr;
        std::string id;
        std::string label;
        bool stock = false;
        bool history = false;
        int width = 0;
        int height = 0;

        bool operator == (const MarkerItem& item) const;

        static Glib::RefPtr<MarkerItem> create() {
            return Glib::make_refptr_for_instance(new MarkerItem());
        }

    protected:
        MarkerItem() = default;
    };

    SPMarker* get_current() const;
    Glib::ustring _current_marker_id;

    sigc::signal<void ()> _signal_changed;
    sigc::signal<void ()> _signal_edit;
    double _preview_scale = 0.0;
    Glib::RefPtr<Gtk::Builder> _builder;
    Gtk::FlowBox& _marker_list;
    Gtk::Label& _marker_name;
    Glib::RefPtr<Gio::ListStore<MarkerItem>> _marker_store;
    std::vector<Glib::RefPtr<MarkerItem>> _stock_items;
    std::vector<Glib::RefPtr<MarkerItem>> _history_items;
    std::map<Gtk::Widget*, Glib::RefPtr<MarkerItem>> _widgets_to_markers;
    Gtk::Picture& _preview;
    bool _preview_no_alloc = true;
    Gtk::Button& _link_scale;
    InkSpinButton& _angle_btn;
    InkSpinButton& _scale_x;
    InkSpinButton& _scale_y;
    Gtk::CheckButton& _scale_with_stroke;
    InkSpinButton& _offset_x;
    InkSpinButton& _offset_y;
    InkSpinButton& _marker_alpha;
    Gtk::ToggleButton& _orient_auto_rev;
    Gtk::ToggleButton& _orient_auto;
    Gtk::ToggleButton& _orient_angle;
    Gtk::Button& _orient_flip_horz;
    Gtk::Picture _current_img;
    Gtk::Button& _edit_marker;
    bool _scale_linked = true;
    guint32 _background_color;
    guint32 _foreground_color;
    Glib::ustring _combo_id;
    int _loc;
    OperationBlocker _update;
    SPDocument *_document = nullptr;
    std::unique_ptr<SPDocument> _sandbox;
    InkPropertyGrid _grid;
    WidgetGroup _widgets;

    void update_ui(SPMarker* marker, bool select);
    void update_widgets_from_marker(SPMarker* marker);
    void update_store();
    Glib::RefPtr<MarkerItem> add_separator(bool filler);
    void update_scale_link();
    Glib::RefPtr<MarkerItem> get_active();
    Glib::RefPtr<MarkerItem> find_marker_item(SPMarker* marker);
    void css_changed(GtkCssStyleChange *change) override;
    void update_preview(Glib::RefPtr<MarkerItem> marker_item);
    void update_menu_btn(Glib::RefPtr<MarkerItem> marker_item);
    void set_active(Glib::RefPtr<MarkerItem> item);
    void init_combo();
    void marker_list_from_doc(SPDocument* source, bool history);
    std::vector<SPMarker*> get_marker_list(SPDocument* source);
    void add_markers (std::vector<SPMarker *> const& marker_list, SPDocument *source,  gboolean history);
    void remove_markers (gboolean history);
    Cairo::RefPtr<Cairo::Surface> create_marker_image(Geom::IntPoint pixel_size, gchar const *mname,
        SPDocument *source, Inkscape::Drawing &drawing, unsigned /*visionkey*/, bool checkerboard, bool no_clip, double scale, bool add_cross);
    void refresh_after_markers_modified();
    sigc::scoped_connection modified_connection;
    sigc::scoped_connection _idle;
    bool _is_up_to_date = false;
};

} // namespace Inkscape::UI::Widget

#endif // SEEN_SP_MARKER_COMBO_BOX_H

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
