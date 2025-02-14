//
// Created by Michael Kowalski on 2/13/25.
//

#include "key-helpers.h"

#include <numeric>
#include <gtkmm/accelerator.h>
#include <gdk/gdkkeysyms.h>

namespace Inkscape::Util {

Glib::ustring get_key_label(const Glib::RefPtr<Gdk::Display>& display, int keyval, int keycode, Gdk::ModifierType mod) {
    if (!display) return {};

#ifdef __APPLE__
    // Special treatment for all key combinations with <option> modifier.
    // Option inserts symbols. We need to retrieve key letter in this case,
    // so we can presented 'A' in option+A as 'A' and not an angstrom symbol for instance.

    // was <option> modifier held down?
    if ((mod & Gdk::ModifierType::ALT_MASK) != Gdk::ModifierType::NO_MODIFIER_MASK) {
        // no valid keycode available?
        if (keycode < 0) {
            GdkKeymapKey* map = nullptr;
            int len = 0;
            gdk_display_map_keyval(display->gobj(), keyval, &map, &len);
            if (len > 0) {
                keycode = map[0].keycode;
            }
            g_free(map);
            // for (int i = 0; i < len; ++i) {
            //     printf("code: %d grp: %d lvl: %d \n", map[i].keycode, map[i].group, map[i].level);
            // }
        }

        guint* keys = 0;
        int n = 0;
        gdk_display_map_keycode(display->gobj(), keycode, nullptr /*&keymap*/, &keys, &n);

        if (n > 1) {
            // use normal keyval or with <shift>, but without <option> - option produces symbols
            keyval = keys[(mod & Gdk::ModifierType::SHIFT_MASK) != Gdk::ModifierType::NO_MODIFIER_MASK ? 1 : 0];
        }
        g_free(keys);
    }
#endif

    return Gtk::Accelerator::get_label(keyval, mod);
}

bool is_key_modifier(int keyval) {
    switch (keyval) {
    case GDK_KEY_Shift_L: return true;
    case GDK_KEY_Shift_R: return true;
    case GDK_KEY_Control_L: return true;
    case GDK_KEY_Control_R: return true;
    // case GDK_KEY_Caps_Lock: return true; 0xffe5
    // case GDK_KEY_Shift_Lock: return true; 0xffe6
    case GDK_KEY_Meta_L: return true;
    case GDK_KEY_Meta_R: return true;
    case GDK_KEY_Alt_L: return true;
    case GDK_KEY_Alt_R: return true;
    case GDK_KEY_Super_L: return true;
    case GDK_KEY_Super_R: return true;
    case GDK_KEY_Hyper_L: return true;
    case GDK_KEY_Hyper_R: return true;

    default: return false;
    }
}

Glib::ustring format_accel_keys(const Glib::RefPtr<Gdk::Display>& display, const std::vector<Gtk::AccelKey>& accel) {
    if (accel.empty()) return {};

    Glib::ustring output;
    output.reserve(64); // speculative

    for (auto& key : accel) {
        if (!output.empty()) output += ", ";

        output += get_key_label(display, key.get_key(), -1, key.get_mod());
    }

    return output;
}

} // namespace
