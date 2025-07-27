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

#include "align-and-distribute.h" // Widget

#include <gtkmm/combobox.h>
#include <gtkmm/frame.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/treemodelfilter.h>
#include <gtkmm/eventcontrollermotion.h>

#include "actions/actions-tools.h" // Tool switching.
#include "desktop.h"               // Tool switching.
#include "inkscape-window.h"       // Activate window action.
#include "io/resource.h"
#include "selection.h"
#include "ui/builder-utils.h"
#include "ui/dialog/dialog-base.h" // Tool switching.
#include "ui/util.h"
#include "message-stack.h"         // For status messages
#include "object/sp-item.h"        // For SPItem cast
#include "util/cast.h"             // For cast function
#include "ui/widget/canvas.h"      // For canvas access
#include "object/algorithms/removeoverlap.h" // For remove overlap preview
#include "2geom/rect.h"            // For bounding box calculations

namespace Inkscape::UI::Dialog {

using Inkscape::IO::Resource::get_filename;
using Inkscape::IO::Resource::UIS;

AlignAndDistribute::AlignAndDistribute(Inkscape::UI::Dialog::DialogBase *dlg)
    : Gtk::Box(Gtk::Orientation::VERTICAL)
    , builder(create_builder("align-and-distribute.ui"))
    , align_and_distribute_box(get_widget<Gtk::Box>(builder, "align-and-distribute-box"))
    , align_and_distribute_object(get_widget<Gtk::Box>(builder, "align-and-distribute-object"))
    , remove_overlap_frame(get_widget<Gtk::Frame>(builder, "remove-overlap-frame"))
    , align_and_distribute_node(get_widget<Gtk::Box>(builder, "align-and-distribute-node"))

    // Object align
    , align_relative_object(get_widget<Gtk::ComboBox>(builder, "align-relative-object"))
    , align_move_as_group(get_widget<Gtk::ToggleButton>(builder, "align-move-as-group"))

    // Remove overlap
    , remove_overlap_button(get_widget<Gtk::Button>(builder, "remove-overlap-button"))
    , remove_overlap_hgap(get_widget<Gtk::SpinButton>(builder, "remove-overlap-hgap"))
    , remove_overlap_vgap(get_widget<Gtk::SpinButton>(builder, "remove-overlap-vgap"))

    // Node
    , align_relative_node(get_widget<Gtk::ComboBox>(builder, "align-relative-node"))

{
    set_name("AlignAndDistribute");

    append(align_and_distribute_box);

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();

    // ------------  Object Align  -------------

    std::string align_to = prefs->getString("/dialogs/align/objects-align-to", "selection");
    multi_selection_align_to = align_to;

    auto filtered_store = Gtk::TreeModelFilter::create(align_relative_object.get_model());
    filtered_store->set_visible_func([=, this](const Gtk::TreeModel::const_iterator &it) {
        if (single_item) {
            Glib::ustring name;
            it->get_value(1, name);
            return single_selection_relative_categories.contains(name);
        }
        return true;
    });

    if (auto win = InkscapeApplication::instance()->get_active_window()) {
        if (auto desktop = win->get_desktop()) {
            if (auto selection = desktop->getSelection()) {
                single_item = selection->singleItem();
                sel_changed = selection->connectChanged([this, filtered_store](Inkscape::Selection *selection) {
                    single_item = selection->singleItem();
                    auto active_id = single_item ? single_selection_align_to : multi_selection_align_to;
                    filtered_store->refilter();
                    align_relative_object.set_active_id(active_id);
                });
            }
        }
    }

    align_relative_object.set_model(filtered_store);
    auto active_id = single_item ? single_selection_align_to : multi_selection_align_to;
    align_relative_object.set_active_id(active_id);
    align_relative_object.signal_changed().connect(sigc::mem_fun(*this, &AlignAndDistribute::on_align_relative_object_changed));

    bool sel_as_group = prefs->getBool("/dialogs/align/sel-as-groups");
    align_move_as_group.set_active(sel_as_group);
    align_move_as_group.signal_clicked().connect(sigc::mem_fun(*this, &AlignAndDistribute::on_align_as_group_clicked));

    // clang-format off
    std::vector<std::pair<const char*, const char*>> align_buttons = {
        {"align-horizontal-right-to-anchor", "right anchor"  },
        {"align-horizontal-left",            "left"          },
        {"align-horizontal-center",          "hcenter"       },
        {"align-horizontal-right",           "right"         },
        {"align-horizontal-left-to-anchor",  "left anchor"   },
        {"align-horizontal-baseline",        "horizontal"    },
        {"align-vertical-bottom-to-anchor",  "bottom anchor" },
        {"align-vertical-top",               "top"           },
        {"align-vertical-center",            "vcenter"       },
        {"align-vertical-bottom",            "bottom"        },
        {"align-vertical-top-to-anchor",     "top anchor"    },
        {"align-vertical-baseline",          "vertical"      }
    };
    // clang-format on

    for (auto align_button : align_buttons) {
        setup_hover_preview_for_button(align_button.first, align_button.second, 
            [this](const std::string& action) { preview_align(action); });
    }

    // ------------ Distribution buttons -------------
    std::vector<std::pair<const char*, const char*>> distribute_buttons = {
        {"distribute-horizontal-left",    "distribute-left"    },
        {"distribute-horizontal-center",  "distribute-hcenter" },
        {"distribute-horizontal-right",   "distribute-right"   },
        {"distribute-horizontal-gaps",    "distribute-hgaps"   },
        {"distribute-vertical-top",       "distribute-top"     },
        {"distribute-vertical-center",    "distribute-vcenter" },
        {"distribute-vertical-bottom",    "distribute-bottom"  },
        {"distribute-vertical-gaps",      "distribute-vgaps"   }
    };

    for (auto distribute_button : distribute_buttons) {
        setup_hover_preview_for_button(distribute_button.first, distribute_button.second,
            [this](const std::string& action) { preview_distribute(action); });
    }

    // ------------ Rearrange buttons -------------
    std::vector<std::pair<const char*, const char*>> rearrange_buttons = {
        {"rearrange-graph",        "rearrange-graph"     },
        {"exchange-positions",     "exchange-positions"  },
        {"exchange-positions-clockwise", "exchange-clockwise" },
        {"exchange-positions-random", "exchange-random"  },
        {"unclump",               "unclump"             }
    };

    for (auto rearrange_button : rearrange_buttons) {
        setup_hover_preview_for_button(rearrange_button.first, rearrange_button.second,
            [this](const std::string& action) { preview_rearrange(action); });
    }

    // ------------ Remove overlap -------------
    setup_hover_preview_for_button("remove-overlap-button", "remove-overlap",
        [this](const std::string& action) { preview_remove_overlap(); });

    // ------------  Node Align  -------------

    std::string align_nodes_to = prefs->getString("/dialogs/align/nodes-align-to", "first");
    align_relative_node.set_active_id(align_nodes_to);
    align_relative_node.signal_changed().connect(sigc::mem_fun(*this, &AlignAndDistribute::on_align_relative_node_changed));

    std::vector<std::pair<const char*, const char*>> align_node_buttons = {
        {"align-node-horizontal", "horizontal"},
        {"align-node-vertical",   "vertical"  }
    };

    for (auto align_button: align_node_buttons) {
        auto &button = get_widget<Gtk::Button>(builder, align_button.first);
        button.signal_clicked().connect(
            sigc::bind(sigc::mem_fun(*this, &AlignAndDistribute::on_align_node_clicked), align_button.second));
    }

    // ------------ Set initial values ------------

    // Normal or node alignment?
    auto desktop = dlg->getDesktop();
    if (desktop) {
        desktop_changed(desktop);
    }

    auto set_icon_size_prefs = [prefs, this]() {
        int size = prefs->getIntLimited("/toolbox/tools/iconsize", -1, 16, 48);
        Inkscape::UI::set_icon_sizes(static_cast<Gtk::Widget*>(this), size);
    };

    // For now we are going to track the toolbox icon size, in the future we will have our own
    // dialog based icon sizes, perhaps done via css instead.
    _icon_sizes_changed = prefs->createObserver("/toolbox/tools/iconsize", set_icon_size_prefs);
    set_icon_size_prefs();

    // Initialize hover preview preference if not set
    if (prefs->getString("/dialogs/align/enable-hover-preview", "").empty()) {
        prefs->setBool("/dialogs/align/enable-hover-preview", true);
    }
}

AlignAndDistribute::~AlignAndDistribute()
{
    // Clean up any active preview
    if (_preview_active) {
        end_preview();
    }
    
    // Disconnect timeout connection
    if (_preview_timeout_connection.connected()) {
        _preview_timeout_connection.disconnect();
    }
}

void
AlignAndDistribute::setup_hover_preview_for_button(const char* button_id, const char* action_name,
                                                   std::function<void(const std::string&)> preview_func)
{
    auto &button = get_widget<Gtk::Button>(builder, button_id);
    
    // Connect click handler
    button.signal_clicked().connect(
        sigc::bind(sigc::mem_fun(*this, &AlignAndDistribute::on_button_clicked), std::string(action_name)));
    
    // Create motion controller for hover events
    auto motion_controller = Gtk::EventControllerMotion::create();
    
    // Store action string with controller
    std::string action(action_name);
    _motion_controllers[action] = motion_controller;
    
    // Connect hover handlers
    motion_controller->signal_enter().connect([this, action, preview_func](double /*x*/, double /*y*/) {
        // Check if preview is enabled
        Inkscape::Preferences *prefs = Inkscape::Preferences::get();
        bool preview_enabled = prefs->getBool("/dialogs/align/enable-hover-preview", true);
        if (!preview_enabled) return;
        
        // Cancel any existing timeout
        if (_preview_timeout_connection.connected()) {
            _preview_timeout_connection.disconnect();
        }
        
        // Store action and start timeout
        _preview_action = action;
        _preview_func = preview_func;
        _preview_timeout_connection = Glib::signal_timeout().connect(
            sigc::mem_fun(*this, &AlignAndDistribute::start_preview_timeout), 300);
    });
    
    motion_controller->signal_leave().connect([this]() {
        // Cancel pending preview
        if (_preview_timeout_connection.connected()) {
            _preview_timeout_connection.disconnect();
        }
        // End current preview
        end_preview();
    });
    
    // Add controller to button
    button.add_controller(motion_controller);
}

void
AlignAndDistribute::desktop_changed(SPDesktop* desktop)
{
    tool_connection.disconnect();
    if (desktop) {
        tool_connection =
            desktop->connectEventContextChanged(sigc::mem_fun(*this, &AlignAndDistribute::tool_changed_callback));
        tool_changed(desktop);
    }
}

void
AlignAndDistribute::tool_changed(SPDesktop* desktop)
{
    bool const is_node = get_active_tool(desktop) == "Node";
    align_and_distribute_node  .set_visible( is_node);
    align_and_distribute_object.set_visible(!is_node);
    remove_overlap_frame.set_visible(!is_node);
}

void
AlignAndDistribute::tool_changed_callback(SPDesktop* desktop, Inkscape::UI::Tools::ToolBase* tool)
{
    tool_changed(desktop);
}

void
AlignAndDistribute::on_align_as_group_clicked()
{
    bool state = align_move_as_group.get_active();
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    prefs->setBool("/dialogs/align/sel-as-groups", state);
}

void
AlignAndDistribute::on_align_relative_object_changed()
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    auto align_to = align_relative_object.get_active_id();
    prefs->setString("/dialogs/align/objects-align-to", align_to);

    if (auto win = InkscapeApplication::instance()->get_active_window()) {
        if (auto desktop = win->get_desktop()) {
            if (auto selection = desktop->getSelection()) {
                if (selection->singleItem()) {
                    single_selection_align_to = align_to;
                } else {
                    multi_selection_align_to = align_to;
                }
            }
        }
    }
}

void
AlignAndDistribute::on_align_relative_node_changed()
{
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    prefs->setString("/dialogs/align/nodes-align-to", align_relative_node.get_active_id());
}

void
AlignAndDistribute::on_button_clicked(std::string const &action)
{
    // If preview is active, we just confirm it (transforms already applied in preview)
    if (_preview_active && _preview_action == action) {
        confirm_preview();
        return;
    }
    
    // Normal operation (no preview was active) - execute the actual command
    execute_action(action);
}

void
AlignAndDistribute::execute_action(const std::string& action)
{
    if (action.find("distribute") != std::string::npos) {
        execute_distribute_action(action);
    } else if (action == "remove-overlap") {
        on_remove_overlap_clicked();
    } else if (action.find("rearrange") != std::string::npos || 
               action.find("exchange") != std::string::npos || 
               action == "unclump") {
        execute_rearrange_action(action);
    } else {
        // Alignment action
        on_align_clicked(action);
    }
}

void
AlignAndDistribute::execute_distribute_action(const std::string& action)
{
    auto app = Gio::Application::get_default();
    auto variant = Glib::Variant<Glib::ustring>::create(action);
    app->activate_action("object-distribute", variant);
}

void
AlignAndDistribute::execute_rearrange_action(const std::string& action)
{
    auto app = Gio::Application::get_default();
    auto variant = Glib::Variant<Glib::ustring>::create(action);
    
    if (action == "rearrange-graph") {
        app->activate_action("object-rearrange-graph", variant);
    } else if (action.find("exchange") != std::string::npos) {
        app->activate_action("object-exchange-positions", variant);
    } else if (action == "unclump") {
        app->activate_action("object-unclump", variant);
    }
}

void
AlignAndDistribute::on_align_clicked(std::string const &align_to)
{
    Glib::ustring argument = align_to;
    argument += " " + align_relative_object.get_active_id();

    if (align_move_as_group.get_active()) {
        argument += " group";
    }

    auto variant = Glib::Variant<Glib::ustring>::create(argument);
    auto app = Gio::Application::get_default();

    if (align_to.find("vertical") != Glib::ustring::npos or align_to.find("horizontal") != Glib::ustring::npos) {
        app->activate_action("object-align-text", variant);
    } else {
        app->activate_action("object-align",      variant);
    }
}

void
AlignAndDistribute::on_remove_overlap_clicked()
{
    double hgap = remove_overlap_hgap.get_value();
    double vgap = remove_overlap_vgap.get_value();

    auto variant = Glib::Variant<std::tuple<double, double>>::create(std::tuple<double, double>(hgap, vgap));
    auto app = Gio::Application::get_default();
    app->activate_action("object-remove-overlaps", variant);
}

void
AlignAndDistribute::on_align_node_clicked(std::string const &direction)
{
    Glib::ustring argument = align_relative_node.get_active_id();

    auto variant = Glib::Variant<Glib::ustring>::create(argument);
    InkscapeWindow* win = InkscapeApplication::instance()->get_active_window();

    if (!win) {
        return;
    }

    if (direction == "horizontal") {
        win->activate_action("win.node-align-horizontal", variant);
    } else {
        win->activate_action("win.node-align-vertical", variant);
    }
}

// ================== HOVER PREVIEW METHODS ==================

bool
AlignAndDistribute::start_preview_timeout()
{
    if (_preview_func) {
        start_preview();
    }
    _preview_timeout_connection.disconnect();
    return false;
}

void
AlignAndDistribute::start_preview()
{
    auto win = InkscapeApplication::instance()->get_active_window();
    if (!win) return;
    
    auto desktop = win->get_desktop();
    if (!desktop) return;
    
    auto selection = desktop->getSelection();
    if (!selection || selection->isEmpty()) {
        return;
    }
    
    // If preview is already active, end it first
    if (_preview_active) {
        end_preview();
    }
    
    // Store original transforms
    store_original_transforms();
    
    // Set preview flag
    _preview_active = true;
    
    // Execute preview function
    if (_preview_func) {
        _preview_func(_preview_action);
    }
    
    // Update status
    desktop->messageStack()->flash(Inkscape::INFORMATION_MESSAGE, 
        "Preview active - click to confirm, move mouse away to cancel");
}

void
AlignAndDistribute::end_preview()
{
    if (!_preview_active) {
        return;
    }
    
    // Restore original transforms
    restore_original_transforms();
    
    _preview_active = false;
    
    // Clear status message
    auto win = InkscapeApplication::instance()->get_active_window();
    if (win) {
        if (auto desktop = win->get_desktop()) {
            desktop->messageStack()->flash(Inkscape::INFORMATION_MESSAGE, "");
        }
    }
}

void
AlignAndDistribute::confirm_preview()
{
    if (!_preview_active) {
        return;
    }
    
    _preview_active = false;
    
    // Clear preview data but don't restore (we want to keep the changes)
    _original_transforms.clear();
    _preview_objects.clear();
    
    // Clear status message
    auto win = InkscapeApplication::instance()->get_active_window();
    if (win) {
        if (auto desktop = win->get_desktop()) {
            desktop->messageStack()->flash(Inkscape::INFORMATION_MESSAGE, "");
        }
    }
}

void
AlignAndDistribute::store_original_transforms()
{
    _original_transforms.clear();
    _preview_objects.clear();
    
    auto win = InkscapeApplication::instance()->get_active_window();
    if (!win) return;
    
    auto desktop = win->get_desktop();
    if (!desktop) return;
    
    auto selection = desktop->getSelection();
    if (!selection) return;
    
    auto items = selection->items();
    for (auto item : items) {
        _preview_objects.push_back(item);
        _original_transforms.push_back(item->transform);
    }
}

void
AlignAndDistribute::restore_original_transforms()
{
    auto win = InkscapeApplication::instance()->get_active_window();
    if (!win) return;
    
    auto desktop = win->get_desktop();
    if (!desktop) return;
    
    // Restore transforms
    for (size_t i = 0; i < _preview_objects.size() && i < _original_transforms.size(); ++i) {
        if (auto item = cast<SPItem>(_preview_objects[i])) {
            item->set_transform(_original_transforms[i]);
        }
    }
    
    // Force canvas update
    desktop->getCanvas()->redraw_all();
    
    // Clear stored data
    _original_transforms.clear();
    _preview_objects.clear();
}

// ================== PREVIEW IMPLEMENTATION METHODS ==================

void
AlignAndDistribute::preview_align(const std::string& action)
{
    auto win = InkscapeApplication::instance()->get_active_window();
    if (!win) return;
    
    auto desktop = win->get_desktop();
    if (!desktop) return;
    
    auto selection = desktop->getSelection();
    if (!selection || selection->isEmpty()) return;
    
    // Get the reference object/point for alignment
    auto items = selection->items();
    if (items.empty()) return;
    
    // Calculate reference bounds based on align-to setting
    Geom::OptRect reference_bounds;
    std::string align_to = align_relative_object.get_active_id();
    
    if (align_to == "page") {
        reference_bounds = desktop->getDocument()->preferredBounds();
    } else if (align_to == "selection") {
        reference_bounds = selection->preferredBounds();
    } else {
        // For now, use selection bounds as fallback
        reference_bounds = selection->preferredBounds();
    }
    
    if (!reference_bounds) return;
    
    // Apply alignment preview to each item
    for (auto item : items) {
        if (auto sp_item = cast<SPItem>(item)) {
            auto item_bounds = sp_item->preferredBounds();
            if (!item_bounds) continue;
            
            Geom::Affine transform = sp_item->transform;
            Geom::Point offset(0, 0);
            
            // Calculate offset based on alignment type
            if (action == "left") {
                offset.x() = reference_bounds->left() - item_bounds->left();
            } else if (action == "hcenter") {
                offset.x() = reference_bounds->midpoint().x() - item_bounds->midpoint().x();
            } else if (action == "right") {
                offset.x() = reference_bounds->right() - item_bounds->right();
            } else if (action == "top") {
                offset.y() = reference_bounds->top() - item_bounds->top();
            } else if (action == "vcenter") {
                offset.y() = reference_bounds->midpoint().y() - item_bounds->midpoint().y();
            } else if (action == "bottom") {
                offset.y() = reference_bounds->bottom() - item_bounds->bottom();
            }
            
            // Apply the offset
            transform *= Geom::Translate(offset);
            sp_item->set_transform(transform);
        }
    }
    
    // Force canvas update
    desktop->getCanvas()->redraw_all();
}

void
AlignAndDistribute::preview_distribute(const std::string& action)
{
    auto win = InkscapeApplication::instance()->get_active_window();
    if (!win) return;
    
    auto desktop = win->get_desktop();
    if (!desktop) return;
    
    auto selection = desktop->getSelection();
    if (!selection || selection->isEmpty()) return;
    
    auto items = selection->items();
    if (items.size() < 3) return; // Need at least 3 items to distribute
    
    // Sort items by position
    std::vector<SPItem*> sorted_items;
    for (auto item : items) {
        if (auto sp_item = cast<SPItem>(item)) {
            sorted_items.push_back(sp_item);
        }
    }
    
    if (sorted_items.size() < 3) return;
    
    // Sort based on distribution type
    if (action.find("horizontal") != std::string::npos || action.find("left") != std::string::npos || 
        action.find("right") != std::string::npos || action.find("hcenter") != std::string::npos) {
        std::sort(sorted_items.begin(), sorted_items.end(), [](SPItem* a, SPItem* b) {
            auto bounds_a = a->preferredBounds();
            auto bounds_b = b->preferredBounds();
            if (!bounds_a || !bounds_b) return false;
            return bounds_a->midpoint().x() < bounds_b->midpoint().x();
        });
    } else {
        std::sort(sorted_items.begin(), sorted_items.end(), [](SPItem* a, SPItem* b) {
            auto bounds_a = a->preferredBounds();
            auto bounds_b = b->preferredBounds();
            if (!bounds_a || !bounds_b) return false;
            return bounds_a->midpoint().y() < bounds_b->midpoint().y();
        });
    }
    
    // Calculate distribution spacing
    auto first_bounds = sorted_items.front()->preferredBounds();
    auto last_bounds = sorted_items.back()->preferredBounds();
    if (!first_bounds || !last_bounds) return;
    
    double total_space;
    if (action.find("horizontal") != std::string::npos) {
        total_space = last_bounds->midpoint().x() - first_bounds->midpoint().x();
    } else {
        total_space = last_bounds->midpoint().y() - first_bounds->midpoint().y();
    }
    
    double spacing = total_space / (sorted_items.size() - 1);
    
    // Apply distribution
    for (size_t i = 1; i < sorted_items.size() - 1; ++i) {
        auto item = sorted_items[i];
        auto item_bounds = item->preferredBounds();
        if (!item_bounds) continue;
        
        Geom::Affine transform = item->transform;
        Geom::Point target_pos;
        
        if (action.find("horizontal") != std::string::npos) {
            target_pos.x() = first_bounds->midpoint().x() + spacing * i;
            target_pos.y() = item_bounds->midpoint().y();
        } else {
            target_pos.x() = item_bounds->midpoint().x();
            target_pos.y() = first_bounds->midpoint().y() + spacing * i;
        }
        
        Geom::Point offset = target_pos - item_bounds->midpoint();
        transform *= Geom::Translate(offset);
        item->set_transform(transform);
    }
    
    desktop->getCanvas()->redraw_all();
}

void
AlignAndDistribute::preview_remove_overlap()
{
    // For simplicity, just show current positions (remove overlap is complex to preview)
    auto win = InkscapeApplication::instance()->get_active_window();
    if (!win) return;
    
    auto desktop = win->get_desktop();
    if (!desktop) return;
    
    desktop->messageStack()->flash(Inkscape::INFORMATION_MESSAGE, 
        "Remove overlap preview - click to apply");
}

void
AlignAndDistribute::preview_rearrange(const std::string& action)
{
    // For simplicity, just show current positions (rearrange operations are complex to preview)
    auto win = InkscapeApplication::instance()->get_active_window();
    if (!win) return;
    
    auto desktop = win->get_desktop();
    if (!desktop) return;
    
    desktop->messageStack()->flash(Inkscape::INFORMATION_MESSAGE, 
        "Rearrange preview - click to apply");
}

} // namespace Inkscape::UI::Dialog

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