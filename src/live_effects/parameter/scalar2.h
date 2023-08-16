// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef INKSCAPE_LIVEPATHEFFECT_SPINBUTTON_H
#define INKSCAPE_LIVEPATHEFFECT_SPINBUTTON_H

/*
 * Inkscape::LivePathEffectParameters
 *
 * Copyright (C) Johan Engelen 2007 <j.b.c.engelen@utwente.nl>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#include "live_effects/parameter/parameter.h"
#include <glibmm/ustring.h>
#include "ui/widget/registered-widget.h"

// In gtk2, this wasn't an issue; we could toss around
// G_MAXDOUBLE and not worry about size allocations. But
// in gtk3, it is an issue: it allocates widget size for the maxmium
// value you pass to it, leading to some insane lengths.
// If you need this to be more, please be conservative about it.
const double Scalar2Param_G_MAXDOUBLE =
    10000000000.0; // TODO fixme: using an arbitrary large number as a magic value seems fragile.
namespace UI {
namespace Widget {
class Registry;
}
} // namespace UI
namespace Inkscape {
namespace LivePathEffect {
class Effect;
class Scalar2Param : public Parameter {
  public:
    Scalar2Param(const Glib::ustring &label, const Glib::ustring &tip, const Glib::ustring &key,
                Inkscape::UI::Widget::Registry *wr, Effect *effect, gdouble default_value = 1.0);
    ~Scalar2Param() override;
    Scalar2Param(const Scalar2Param &) = delete;
    Scalar2Param &operator=(const Scalar2Param &) = delete;

    bool param_readSVGValue(const gchar *strvalue) override;
    Glib::ustring param_getSVGValue() const override;
    Glib::ustring param_getDefaultSVGValue() const override;
    void param_transform_multiply(Geom::Affine const &postmul, bool set) override;

    void param_set_default() override;
    void param_update_default(gdouble default_value);
    void param_update_default(const gchar *default_value) override;
    void param_set_value(gdouble val);
    void param_make_integer(bool yes = true);
    void param_set_range(gdouble min, gdouble max);
    void param_set_digits(unsigned digits);
    void param_set_increments(double step, double page);
    double param_get_max() { return max; };
    double param_get_min() { return min; };
    void param_set_undo(bool set_undo);
    Gtk::Widget *param_newWidget() override;
    ParamType paramType() const override { return ParamType::SCALAR; };
    inline operator gdouble() const { return value; };

  protected:
    gdouble value;
    gdouble min;
    gdouble max;
    bool integer;
    gdouble defvalue;
    unsigned digits;
    double inc_step;
    double inc_page;
    bool _set_undo;
};

} // namespace LivePathEffect

} // namespace Inkscape

#endif

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99 :
