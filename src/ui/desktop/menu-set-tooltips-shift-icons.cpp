// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Go over a widget representing a menu, & set tooltips on its items from app label-to-tooltip map.
 * Optionally (per Preference) shift Gtk::MenuItems with icons to align with Toggle & Radio buttons
 */
/*
 * Authors:
 *   Tavmjong Bah       <tavmjong@free.fr>
 *   Patrick Storz      <eduard.braun2@gmx.de>
 *   Daniel Boles       <dboles.src+inkscape@gmail.com>
 *
 * Copyright (C) 2020-2023 Authors
 *
 * The contents of this file may be used under the GNU General Public License Version 2 or later.
 * Read the file 'COPYING' for more information.
 *
 */

#include "ui/desktop/menu-set-tooltips-shift-icons.h"

#include <string_view>
using namespace std::literals;

#include <gtkmm/box.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>

#include "inkscape-application.h"  // Action extra data
#include "ui/util.h"

template <typename T>
static T *find_child(Gtk::Widget &parent)
{
    for (auto &child : Inkscape::UI::children(parent)) {
        if (auto t = dynamic_cast<T *>(&child)) {
            return t;
        }
    }
    return nullptr;
}

/**
 * Go over a widget representing a menu, & set tooltips on its items from app label-to-tooltip map.
 * @param shift_icons If true:
 * Install CSS to shift icons into the space reserved for toggles (i.e. check and radio items).
 * The CSS will apply to all menu icons but is updated as each menu is shown.
 * @returns whether icons were shifted during this or an inner recursive call
 */
bool set_tooltips_and_shift_icons(Gtk::Widget &menu, bool shift_icons)
{
    bool shifted = false;

    auto app = InkscapeApplication::instance();
    auto &label_to_tooltip_map = app->get_menu_label_to_tooltip_map();

    for (auto &child : Inkscape::UI::children(menu)) {
        if (gtk_widget_get_name(child.gobj()) == "GtkModelButton"sv) {

            // The ModelButton contains in order: GtkBox, GtkImage (optionally), GtkLabel, GtkPopoverMenu (optionally).

            // Set tooltip on GtkModelButton.
            if (auto label_widget = find_child<Gtk::Label>(child)) {
                auto label = label_widget->get_label();
                if (!label.empty()) {
                    auto it = label_to_tooltip_map.find(label);
                    if (it != label_to_tooltip_map.end()) {
                        child.set_tooltip_text(it->second);
                    }
                }
            }

            // Make GtkImage visible and move to start of GtkBox.
            if (auto image = find_child<Gtk::Image>(child)) {
                if (auto box = find_child<Gtk::Box>(child)) {
                    image->reference();
                    image->unparent();
                    image->insert_at_start(*box);
                    image->unreference();
                    image->set_margin_end(5);
                    image->set_visible();
                    image->property_visible().signal_changed().connect([image] {
                        if (!image->get_visible()) {
                            image->set_visible();
                        }
                    });
                }
            }
        }

        set_tooltips_and_shift_icons(child, shift_icons);
    }

    return shifted;
}

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
