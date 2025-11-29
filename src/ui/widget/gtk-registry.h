// SPDX-License-Identifier: GPL-2.0-or-later
/*
 *
 * Copyright (C) 2025 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#ifndef SEEN_INKSCAPE_UI_WIDGET_GTK_REGISTRY_H
#define SEEN_INKSCAPE_UI_WIDGET_GTK_REGISTRY_H

#include "glibmm/wrap.h"
#include "gtkmm/widget.h"

namespace Inkscape::UI::Widget {

// Add all custom widgets to the Gtk Builder registry so they can be
// Used from glade/ui xml files.
void register_all();

// Construct a C++ object from a parent (=base) C class object
template<class T>
Glib::ObjectBase* wrap_new(GObject* o) {
    auto obj = new T(GTK_WIDGET(o));
    return Gtk::manage(obj);
}

/**
 * Register a "new" type in Glib and bind it to the C++ wrapper function
 */
template<class T>
static void register_type()
{
    if (T::gtype) return;
    auto dummy = T();
    T::gtype = G_OBJECT_TYPE(dummy.Gtk::Widget::gobj());
    Glib::wrap_register(T::gtype, wrap_new<T>);
}

} // namespace Dialog::UI::Widget

#endif // SEEN_INKSCAPE_UI_WIDGET_GTK_REGISTRY_H

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
