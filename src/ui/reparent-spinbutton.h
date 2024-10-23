// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef REPARENT_SPINBUTTON_H
#define REPARENT_SPINBUTTON_H

#include <gtkmm/spinbutton.h>

// Replace SpinButton with InkSpinButton. Removes original SpinButton from its parent
// and places new instance of InkSpinButton in its place, retaining relevant attributes

namespace Inkscape::UI::Widget {
class InkSpinButton;
}

namespace Inkscape::UI {

void replace_spinbutton_widget(Gtk::SpinButton& button, Widget::InkSpinButton& replacement);

Widget::InkSpinButton* replace_spinbutton_widget(Gtk::SpinButton& button);

}

#endif //REPARENT_SPINBUTTON_H
