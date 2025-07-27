// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Align and Distribute widget
 */
/* Authors:
 *   Tavmjong Bah
 *
 *   Based on dialog by:
 *     Bryce W. Harrington <bryce@bryceharrington.org>
 *     Aubanel MONNIER <aubi@libertysurf.fr>
 *     Frank Felfe <innerspace@iname.com>
 *     Lauris Kaplinski <lauris@kaplinski.com>
 *
 * Copyright (C) 2021 Tavmjong Bah
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_DIALOG_ALIGN_AND_DISTRIBUTE_H
#define INKSCAPE_UI_DIALOG_ALIGN_AND_DISTRIBUTE_H

#include <gtkmm/box.h>
#include <gtkmm/builder.h>
#include <gtkmm/button.h>
#include <gtkmm/combobox.h>
#include <gtkmm/frame.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/eventcontroller.h>
#include <gtkmm/eventcontrollermotion.h>

#include <set>
#include <vector>
#include <map>
#include <functional>
#include <sigc++/connection.h>

#include "2geom/affine.h"
#include "preferences.h"
#include "ui/tools/tool-base.h"

class SPDesktop;
class SPObject;

namespace Inkscape::UI::Dialog {

class DialogBase;

class AlignAndDistribute : public Gtk::Box
{
public:
    AlignAndDistribute(Inkscape::UI::Dialog::DialogBase *dlg);
    ~AlignAndDistribute();

    void desktop_changed(SPDesktop* desktop);

private:
    void tool_changed(SPDesktop* desktop);
    void tool_changed_callback(SPDesktop* desktop, Inkscape::UI::Tools::ToolBase* tool);

    void on_align_as_group_clicked();
    void on_align_relative_object_changed();
    void on_align_relative_node_changed();
    void on_align_clicked(std::string const &align_to);
    void on_remove_overlap_clicked();
    void on_align_node_clicked(std::string const &direction);

    // Button handling
    void on_button_clicked(std::string const &action);
    void execute_action(const std::string& action);
    void execute_distribute_action(const std::string& action);
    void execute_rearrange_action(const std::string& action);
    
    // Hover preview system
    void setup_hover_preview_for_button(const char* button_id, const char* action_name,
                                       std::function<void(const std::string&)> preview_func);
    
    // Preview methods
    bool start_preview_timeout();
    void start_preview();
    void end_preview();
    void confirm_preview();
    void store_original_transforms();
    void restore_original_transforms();
    
    // Preview implementations
    void preview_align(const std::string& action);
    void preview_distribute(const std::string& action);
    void preview_remove_overlap();
    void preview_rearrange(const std::string& action);

    // UI
    Glib::RefPtr<Gtk::Builder> builder;
    Gtk::Box &align_and_distribute_box;
    Gtk::Box &align_and_distribute_object;
    Gtk::Frame &remove_overlap_frame;
    Gtk::Box &align_and_distribute_node;

    // Object align
    Gtk::ComboBox &align_relative_object;
    Gtk::ToggleButton &align_move_as_group;

    // Remove overlap
    Gtk::Button &remove_overlap_button;
    Gtk::SpinButton &remove_overlap_hgap;
    Gtk::SpinButton &remove_overlap_vgap;

    // Node
    Gtk::ComboBox &align_relative_node;

    // State
    sigc::connection tool_connection;
    sigc::connection sel_changed;
    std::unique_ptr<Inkscape::Preferences::PreferencesObserver> _icon_sizes_changed;
    
    bool single_item = false;
    std::string single_selection_align_to = "first";
    std::string multi_selection_align_to = "selection";
    
    std::set<Glib::ustring> single_selection_relative_categories = {
        "first", "last", "biggest", "smallest"
    };
    
    // Preview state
    bool _preview_active = false;
    std::string _preview_action;
    std::function<void(const std::string&)> _preview_func;
    sigc::connection _preview_timeout_connection;
    
    // Store original transforms for preview
    std::vector<SPObject*> _preview_objects;
    std::vector<Geom::Affine> _original_transforms;
    
    // Motion controllers for hover detection
    std::map<std::string, Glib::RefPtr<Gtk::EventControllerMotion>> _motion_controllers;
};

} // namespace Inkscape::UI::Dialog

#endif // INKSCAPE_UI_DIALOG_ALIGN_AND_DISTRIBUTE_H

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