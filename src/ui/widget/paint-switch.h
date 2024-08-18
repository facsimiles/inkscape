// SPDX-License-Identifier: GPL-2.0-or-later

// Simple paint selector widget
// https://gitlab.com/inkscape/ux/-/issues/246

#ifndef PAINT_SWITCH_H
#define PAINT_SWITCH_H

#include <glibmm/ustring.h>
#include <gtkmm/box.h>
#include <gtkmm/stack.h>
#include <memory>
#include <sigc++/connection.h>
#include <sigc++/signal.h>

#include "style-internal.h"
#include "colors/color.h"

namespace Inkscape::UI::Widget {

enum class PaintMode {
    None,   // set to no paint
    Solid,
    Gradient,
    Mesh,
    Pattern,
    Swatch,
    NotSet
};

PaintMode get_mode_from_paint(const SPIPaint& paint);
Glib::ustring get_paint_mode_icon(PaintMode mode);
Glib::ustring get_paint_mode_name(PaintMode mode);

class PaintSwitch : public Gtk::Box {
public:
    PaintSwitch();

    static std::unique_ptr<PaintSwitch> create();

    virtual void set_mode(PaintMode mode) = 0;

    virtual void update_from_paint(const SPIPaint& paint) = 0;

    // flat colors
    virtual void set_color(const Colors::Color& color) = 0;
    virtual sigc::signal<void (const Colors::Color&)> signal_color_changed() = 0;
    virtual sigc::signal<void (PaintMode)> signal_mode_changed() = 0;
};

} // namespace

#endif
