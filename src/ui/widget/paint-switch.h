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

#include "edit-operation.h"
#include "style-internal.h"
#include "colors/color.h"
#include "object/sp-gradient.h"

namespace Inkscape::UI::Widget {

enum class PaintMode {
    Solid,
    Swatch,
    Gradient,
    Mesh,
    Pattern,
    NotSet,
    None,   // set to no paint
};

enum class FillRule {
    NonZero,
    EvenOdd
};

PaintMode get_mode_from_paint(const SPIPaint& paint);
Glib::ustring get_paint_mode_icon(PaintMode mode);
Glib::ustring get_paint_mode_name(PaintMode mode);

class PaintSwitch : public Gtk::Box {
public:
    PaintSwitch();

    // create a new PaintSwitch widget; if 'support_no_paint' is true, then add the "no paint" toggle button too
    static std::unique_ptr<PaintSwitch> create(bool support_no_paint, bool support_fill_rule);

    virtual void set_desktop(SPDesktop* desktop) = 0;
    virtual void set_document(SPDocument* document) = 0;
    virtual void set_mode(PaintMode mode) = 0;
    virtual void update_from_paint(const SPIPaint& paint) = 0;
    virtual void set_fill_rule(FillRule fill_rule) = 0;

    // flat colors
    virtual void set_color(const Colors::Color& color) = 0;
    virtual sigc::signal<void (const Colors::Color&)> get_flat_color_changed() = 0;
    virtual sigc::signal<void (PaintMode)> get_signal_mode_changed() = 0;
    virtual sigc::signal<void (SPGradient* gradient, SPGradientType type)> get_gradient_changed() = 0;
    virtual sigc::signal<void (SPGradient* swatch, EditOperation, SPGradient*, std::optional<Colors::Color>, Glib::ustring)> get_swatch_changed() = 0;
    virtual sigc::signal<void (SPPattern* pattern, std::optional<Colors::Color> color, const Glib::ustring& label,
        const Geom::Affine& transform, const Geom::Point& offset, bool uniform_scale, const Geom::Scale& gap)> get_pattern_changed() = 0;
    virtual sigc::signal<void (SPGradient* mesh)> get_mesh_changed() = 0;
    virtual sigc::signal<void (FillRule)> get_fill_rule_changed() = 0;
};

} // namespace

#endif
