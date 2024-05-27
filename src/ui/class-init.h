// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef INK_CLASS_INIT_H
#define INK_CLASS_INIT_H

#include <glibmm/extraclassinit.h>
#include <gtkmm.h>
#include <sigc++/scoped_connection.h>

namespace Inkscape::Util {

// This is helper class for custom widgets.
// It allows us to modify element name of our new custom widget.
// Element name will replace generic <widget> name and can be used in CSS style sheets.

class ClassExtraInit : public Glib::ExtraClassInit {
public:
    ClassExtraInit(const Glib::ustring& css_name)
      : Glib::ExtraClassInit(my_extra_class_init_function, &_css_name,
                             my_instance_init_function),
        _css_name(css_name) {}

private:
    static void my_extra_class_init_function(void* g_class, void* class_data) {
        const auto klass = static_cast<GtkWidgetClass*>(g_class);
        const auto css_name = static_cast<Glib::ustring*>(class_data);
        gtk_widget_class_set_css_name(klass, css_name->c_str());
    }

    static void my_instance_init_function(GTypeInstance* instance, void* g_class) {
        // no op
    }

    Glib::ustring _css_name;
};

} // namespace

#endif
