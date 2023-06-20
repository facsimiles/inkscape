// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * TODO: insert short description here
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2018 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_TOOLBAR_H
#define SEEN_TOOLBAR_H

#include <gtkmm/box.h>
#include <gtkmm/builder.h>
#include <gtkmm/toolbar.h>
#include <stack>

#include "ui/widget/toolbar-menu-button.h"

namespace Glib {
class ustring;
} // namespace Glib

namespace Gtk {
class Label;
class ToggleToolButton;
class ToolItem;
} // namespace Gtk;

class SPDesktop;

namespace Inkscape::UI::Toolbar {

/**
 * \brief An abstract definition for a toolbar within Inkscape
 *
 * \detail This is basically the same as a Gtk::Toolbar but contains a
 *         few convenience functions. All toolbars must define a "create"
 *         function that adds all the required tool-items and returns the
 *         toolbar as a GtkWidget
 */

class Toolbar : public Gtk::Box
{
public:
    Gtk::Box *_toolbar;
    std::stack<Inkscape::UI::Widget::ToolbarMenuButton *> _expanded_menu_btns;
    std::stack<Inkscape::UI::Widget::ToolbarMenuButton *> _collapsed_menu_btns;

protected:
    SPDesktop *_desktop;

    /**
     * \brief A default constructor that just assigns the desktop
     */
    Toolbar(SPDesktop *desktop);

    Gtk::ToolItem         * add_label(const Glib::ustring &label_text);
    Gtk::ToggleButton *add_toggle_button(const Glib::ustring &label_text, const Glib::ustring &tooltip_text);
    void add_separator();
    Glib::RefPtr<Gtk::Builder> initialize_builder(const char *file_name);
    void resize_handler(Gtk::Allocation &allocation);
    void move_children(Gtk::Box *src, Gtk::Box *dest, std::vector<std::pair<int, Gtk::Widget *>> children,
                       bool is_expanding = false);

protected:
    static GtkWidget * create(SPDesktop *desktop);
};

} // namespace Inkscape::UI::Toolbar

#endif // SEEN_TOOLBAR_H

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
