// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Author:
 *   Felipe Corrêa da Silva Sanches <juca@members.fsf.org>
 *   Tavmjong Bah <tavmjong@free.fr>
 *
 * Copyright (C) 2018 Felipe Corrêa da Silva Sanches, Tavmong Bah
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef INKSCAPE_UI_WIDGET_FONT_VARIATIONS_H
#define INKSCAPE_UI_WIDGET_FONT_VARIATIONS_H

#include <gtkmm/grid.h>
#include <gtkmm/sizegroup.h>
#include <gtkmm/label.h>
#include <gtkmm/scale.h>
#include <gtkmm/spinbutton.h>

#include "libnrtype/OpenTypeUtil.h"

#include "style.h"

namespace Inkscape {
namespace UI {
namespace Widget {


/**
 * A widget for a single axis: Label and Slider
 */
class FontVariationAxis : public Gtk::Grid
{
public:
    FontVariationAxis(Glib::ustring name, OTVarAxis const &axis, Glib::ustring label, Glib::ustring tooltip);
    Glib::ustring get_name() { return name; }
    Gtk::Label* get_label()  { return label; }
    double get_value()       { return scale->get_adjustment()->get_value(); }
    int get_precision()      { return precision; }
    Gtk::Scale* get_scale()  { return scale; }
    double get_def()         { return def; }
    Gtk::SpinButton* get_editbox() { return edit; }

private:

    // Widgets
    Glib::ustring name;
    Gtk::Label* label;
    Gtk::Scale* scale;
    Gtk::SpinButton* edit = nullptr;

    int precision;
    double def = 0.0; // Default value

    // Signals
    sigc::signal<void ()> signal_changed;
};

/**
 * A widget for selecting font variations (OpenType Variations).
 */
class FontVariations : public Gtk::Grid
{

public:

    /**
     * Constructor
     */
    FontVariations();

protected:

public:

    /**
     * Update GUI.
     */
    void update(const Glib::ustring& font_spec);

    /**
     * Fill SPCSSAttr based on settings of buttons.
     */
    void fill_css( SPCSSAttr* css );

    /**
     * Get CSS String
     */
    Glib::ustring get_css_string();

    Glib::ustring get_pango_string(bool include_defaults = false) const;

    void on_variations_change();

    /**
     * Let others know that user has changed GUI settings.
     * (Used to enable 'Apply' and 'Default' buttons.)
     */
    sigc::connection connectChanged(sigc::slot<void ()> slot) {
        return signal_changed.connect(slot);
    }

    // return true if there are some variations present
    bool variations_present() const;

    Glib::RefPtr<Gtk::SizeGroup> get_size_group(int index);

private:

    std::vector<FontVariationAxis*> axes;
    Glib::RefPtr<Gtk::SizeGroup> size_group;
    Glib::RefPtr<Gtk::SizeGroup> size_group_edit;
    sigc::signal<void ()> signal_changed;
};

 
} // namespace Widget
} // namespace UI
} // namespace Inkscape

#endif // INKSCAPE_UI_WIDGET_FONT_VARIATIONS_H

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8 :
