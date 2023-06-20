// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Mesh aux toolbar
 *
 * Authors:
 *   bulia byak <bulia@dr.com>
 *   Tavmjong Bah <tavmjong@free.fr>
 *
 * Copyright (C) 2012 authors
 * Copyright (C) 2005 authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_MESH_TOOLBAR_H
#define SEEN_MESH_TOOLBAR_H

#include <vector>
#include <glibmm/refptr.h>

#include "toolbar.h"

class SPDesktop;
class SPObject;

namespace Gtk {
class Adjustment;
class RadioToolButton;
class ToggleToolButton;
} // namespace Gtk

namespace Inkscape {

class Selection;

namespace UI {

class SimplePrefPusher;

namespace Tools {
class ToolBase;
} // namespace Tools

namespace Widget {
class ComboToolItem;
class SpinButtonToolItem;
} // namespace Widget

namespace Toolbar {

class MeshToolbar final : public Toolbar {
    std::vector<Gtk::RadioToolButton *> _new_type_buttons;
    std::vector<Gtk::RadioToolButton *> _new_fillstroke_buttons;
    UI::Widget::ComboToolItem *_select_type_item;

    Gtk::ToggleButton *_edit_fill_item;
    Gtk::ToggleButton *_edit_stroke_item;

    Glib::RefPtr<Gtk::Adjustment> _row_adj;
    Glib::RefPtr<Gtk::Adjustment> _col_adj;

    std::unique_ptr<UI::SimplePrefPusher> _edit_fill_pusher;
    std::unique_ptr<UI::SimplePrefPusher> _edit_stroke_pusher;
    std::unique_ptr<UI::SimplePrefPusher> _show_handles_pusher;

    sigc::connection c_selection_changed;
    sigc::connection c_selection_modified;
    sigc::connection c_subselection_changed;
    sigc::connection c_defs_release;
    sigc::connection c_defs_modified;

    void new_geometry_changed(int mode);
    void new_fillstroke_changed(int mode);
    void row_changed();
    void col_changed();
    void toggle_fill_stroke();
    void selection_changed(Inkscape::Selection *selection);
    void toggle_handles();
    void watch_ec(SPDesktop* desktop, Inkscape::UI::Tools::ToolBase* tool);
    void selection_modified(Inkscape::Selection *selection, guint flags);
    void drag_selection_changed(gpointer dragger);
    void defs_release(SPObject *defs);
    void defs_modified(SPObject *defs, guint flags);
    void warning_popup();
    void type_changed(int mode);
    void toggle_sides();
    void make_elliptical();
    void pick_colors();
    void fit_mesh();

    MeshToolbar(SPDesktop *desktop);
    ~MeshToolbar() final;

public:
    static GtkWidget * create(SPDesktop *desktop);
};

} // namespace Toolbar
} // namespace UI
} // namespace Inkscape

#endif /* !SEEN_MESH_TOOLBAR_H */

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
