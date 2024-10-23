// SPDX-License-Identifier: GPL-2.0-or-later

#include "reparent-spinbutton.h"

#include "widget/ink-spin-button.h"

namespace Inkscape::UI {

Widget::InkSpinButton* replace_spinbutton_widget(Gtk::SpinButton& button) {
    auto replacement = Gtk::make_managed<Widget::InkSpinButton>();
    replace_spinbutton_widget(button, *replacement);
    return replacement;
}

void replace_spinbutton_widget(Gtk::SpinButton& button, Widget::InkSpinButton& replacement) {
    replacement.set_adjustment(button.get_adjustment());
    replacement.set_tooltip_text(button.get_tooltip_text());
    replacement.set_digits(button.get_digits());
    replacement.set_margin_start(button.get_margin_start());
    replacement.set_margin_end(button.get_margin_end());
    replacement.set_margin_top(button.get_margin_top());
    replacement.set_margin_bottom(button.get_margin_bottom());
    replacement.set_valign(button.get_valign());
    replacement.set_halign(button.get_halign());
    replacement.set_vexpand(button.get_vexpand());
    replacement.set_hexpand(button.get_hexpand());

    if (auto grid = dynamic_cast<Gtk::Grid*>(button.get_parent())) {
        int col, row, w, h;
        grid->query_child(button, col, row, w, h);
        grid->remove(button);
        grid->attach(replacement, col, row, w, h);
    }
    else if (auto box = dynamic_cast<Gtk::Box*>(button.get_parent())) {
        box->insert_child_after(replacement, button);
        box->remove(button);
    }
    else {
        throw std::runtime_error("Unexpected SpinButton parent type");
    }
}

}
