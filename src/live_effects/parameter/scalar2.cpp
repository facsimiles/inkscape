// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Johan Engelen 2007 <j.b.c.engelen@utwente.nl>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "scalar2.h"
#include "svg/stringstream.h"
#include "live_effects/effect.h"
#include "svg/svg.h"
#include "ui/icon-names.h"
#include <glibmm/i18n.h>
namespace Inkscape {
namespace LivePathEffect {
/*###########################################
 *   REAL PARAM
 */
Scalar2Param::Scalar2Param(const Glib::ustring &label, const Glib::ustring &tip, const Glib::ustring &key,
                         Inkscape::UI::Widget::Registry *wr, Effect *effect, gdouble default_value)
    : Parameter(label, tip, key, wr, effect)
    , value(default_value)
    , min(-Scalar2Param_G_MAXDOUBLE)
    , max(Scalar2Param_G_MAXDOUBLE)
    , integer(false)
    , defvalue(default_value)
    , digits(2)
    , inc_step(0.1)
    , inc_page(1)
    , _set_undo(true)
{
}

Scalar2Param::~Scalar2Param() = default;

bool Scalar2Param::param_readSVGValue(const gchar *strvalue)
{
    double newval;
    unsigned int success = sp_svg_number_read_d(strvalue, &newval);
    if (success == 1) {
        param_set_value(newval);
        return true;
    }
    return false;
}

Glib::ustring Scalar2Param::param_getSVGValue() const
{
    Inkscape::SVGOStringStream os;
    os << value;
    return os.str();
}

Glib::ustring Scalar2Param::param_getDefaultSVGValue() const
{
    Inkscape::SVGOStringStream os;
    os << defvalue;
    return os.str();
}

void Scalar2Param::param_set_default() { param_set_value(defvalue); }

void Scalar2Param::param_update_default(gdouble default_value) { defvalue = default_value; }

void Scalar2Param::param_update_default(const gchar *default_value)
{
    double newval;
    unsigned int success = sp_svg_number_read_d(default_value, &newval);
    if (success == 1) {
        param_update_default(newval);
    }
}

void Scalar2Param::param_transform_multiply(Geom::Affine const &postmul, bool set)
{
    // Check if proportional stroke-width scaling is on
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    bool transform_stroke = prefs ? prefs->getBool("/options/transform/stroke", true) : true;
    if (transform_stroke || set) {
        param_set_value(value * postmul.descrim());
        write_to_SVG();
    }
}

void Scalar2Param::param_set_value(gdouble val)
{
    value = val;
    if (integer)
        value = round(value);
    if (value > max)
        value = max;
    if (value < min)
        value = min;
}

void Scalar2Param::param_set_range(gdouble min, gdouble max)
{
    // if you look at client code, you'll see that many effects
    // has a tendency to set an upper range of Geom::infinity().
    // Once again, in gtk2, this is not a problem. But in gtk3,
    // widgets get allocated the amount of size they ask for,
    // leading to excessively long widgets.
    if (min >= -Scalar2Param_G_MAXDOUBLE) {
        this->min = min;
    } else {
        this->min = -Scalar2Param_G_MAXDOUBLE;
    }
    if (max <= Scalar2Param_G_MAXDOUBLE) {
        this->max = max;
    } else {
        this->max = Scalar2Param_G_MAXDOUBLE;
    }
    param_set_value(value); // reset value to see whether it is in ranges
}

void Scalar2Param::param_make_integer(bool yes)
{
    integer = yes;
    digits = 0;
    inc_step = 1;
    inc_page = 10;
}

void Scalar2Param::param_set_undo(bool set_undo) { _set_undo = set_undo; }

Gtk::Widget *Scalar2Param::param_newWidget()
{
    if (widget_is_visible) {
        Inkscape::UI::Widget::RegisteredScalar2 *rsu = Gtk::manage(new Inkscape::UI::Widget::RegisteredScalar2(
            param_label, param_tooltip, param_key, *param_wr, param_effect->getRepr(), param_effect->getSPDoc()));

        rsu->setValue(value);
        rsu->setDigits(digits);
        rsu->setIncrements(inc_step, inc_page);
        rsu->setRange(min, max);
        rsu->setProgrammatically = false;
        if (_set_undo) {
            rsu->set_undo_parameters(_("Change scalar parameter"), INKSCAPE_ICON("dialog-path-effects"));
        }
        return dynamic_cast<Gtk::Widget *>(rsu);
    } else {
        return nullptr;
    }
}

void Scalar2Param::param_set_digits(unsigned digits) { this->digits = digits; }

void Scalar2Param::param_set_increments(double step, double page)
{
    inc_step = step;
    inc_page = page;
}

} /* namespace LivePathEffect */
} /* namespace Inkscape */

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
