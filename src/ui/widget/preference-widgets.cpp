// SPDX-License-Identifier: GPL-2.0-or-later
//
// Created by Michael Kowalski on 10/24/25.
//

#include "preference-widgets.h"

namespace Inkscape::UI::Widget {

GType PreferenceCheckButton::gtype = 0;

void PreferenceCheckButton::construct() {
    auto set_value = [this]{
        auto path = prop_path.get_value();
        if (!path.empty()) {
            if (get_accessible_role() == Role::RADIO) {
                // radio button
                auto value = Preferences::get()->getInt(path);
                set_active(value == prop_enum.get_value());
            }
            else {
                // check box
                auto value = Preferences::get()->getBool(path);
                set_active(value);
            }
        }
    };
    property_pref_path().signal_changed().connect([set_value] { set_value(); });
    set_value();

printf("PrefCheckBtn %p - path %s\n",this,prop_path.get_value().c_str());
}

GType PreferenceSpinButton::gtype = 0;

void PreferenceSpinButton::construct() {
    auto set_num_value = [this]{
        auto path = prop_path.get_value();
        if (!path.empty()) {
            if (get_digits() == 0) {
                // no decimal digits - use integer
                auto value = Preferences::get()->getInt(path);
                set_value(value);
            }
            else {
                auto value = Preferences::get()->getDouble(path);
                set_value(value);
            }
        }
    };
    property_pref_path().signal_changed().connect([set_num_value] { set_num_value(); });
    set_num_value();

    printf("PrefCheckBtn %p - path %s\n",this,prop_path.get_value().c_str());
}

} // namespace

