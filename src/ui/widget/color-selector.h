// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef COLOR_SELECTOR_H
#define COLOR_SELECTOR_H

#include <gtkmm/box.h>
#include <memory>

namespace Inkscape::UI::Widget {

class ColorSelector : public Gtk::Box {
public:
    static std::unique_ptr<ColorSelector> create();

    ColorSelector();

    //
};

} // namespace

#endif

