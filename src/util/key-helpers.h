//
// Created by Michael Kowalski on 2/13/25.
//

#ifndef KEY_FORMAT_H
#define KEY_FORMAT_H

#include <gdkmm/display.h>
#include <glibmm/ustring.h>

#include "action-accel.h"

namespace Inkscape::Util {

// Get a label for key shortcut; if keycode is not known pass (-1), so it will be guessed from keyval
Glib::ustring get_key_label(const Glib::RefPtr<Gdk::Display>& display, int keyval, int keycode, Gdk::ModifierType mod);

// Return true if keyval represents modifier key
bool is_key_modifier(int keyval);

// Format all accelerator keys
Glib::ustring format_accel_keys(const Glib::RefPtr<Gdk::Display>& display, const std::vector<Gtk::AccelKey>& accel);

} // namespace

#endif //KEY_FORMAT_H
