// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Carl Hetherington <inkscape@carlh.net>
 *   Derek P. Moore <derekm@hackunix.org>
 *   Bryce Harrington <bryce@bryceharrington.org>
 *   Johan Engelen <j.b.c.engelen@alumnus.utwente.nl>
 *
 * Copyright (C) 2004-2011 authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "scalar2.h"
#include "spinbutton2.h"
#include <gtkmm/scale.h>

namespace Inkscape {
namespace UI {
namespace Widget {
namespace Ink2 {
Scalar::Scalar(Glib::ustring const &label, Glib::ustring const &tooltip,
               Glib::ustring const &suffix,
               Glib::ustring const &icon,
               bool mnemonic)
    : Labelled(label, tooltip, new SpinButton(), suffix, icon, mnemonic),
      setProgrammatically(false)
{
}

Scalar::Scalar(Glib::ustring const &label, Glib::ustring const &tooltip,
               unsigned digits,
               Glib::ustring const &suffix,
               Glib::ustring const &icon,
               bool mnemonic)
    : Labelled(label, tooltip, new SpinButton(), suffix, icon, mnemonic),
      setProgrammatically(false)
{
}

Scalar::Scalar(Glib::ustring const &label, Glib::ustring const &tooltip,
               Glib::RefPtr<Gtk::Adjustment> &adjust,
               unsigned digits,
               Glib::ustring const &suffix,
               Glib::ustring const &icon,
               bool mnemonic)
    : Labelled(label, tooltip, new SpinButton(), suffix, icon, mnemonic),
      setProgrammatically(false)
{
}

unsigned Scalar::getDigits() const
{
    g_assert(_widget != nullptr);
    return static_cast<SpinButton*>(_widget)->get_digits();
}

double Scalar::getStep() const
{
    g_assert(_widget != nullptr);
    auto adj = static_cast<SpinButton*>(_widget)->get_adjustment();;
    return adj->get_step_increment();
}

double Scalar::getPage() const
{
    g_assert(_widget != nullptr);
    auto adj = static_cast<SpinButton*>(_widget)->get_adjustment();;
    return adj->get_page_increment();
}

double Scalar::getRangeMin() const
{
    g_assert(_widget != nullptr);
    auto adj = static_cast<SpinButton*>(_widget)->get_adjustment();;
    return adj->get_lower();
}

double Scalar::getRangeMax() const
{
    g_assert(_widget != nullptr);
    auto adj = static_cast<SpinButton*>(_widget)->get_adjustment();;
    return adj->get_upper();
}

double Scalar::getValue() const
{
    g_assert(_widget != nullptr);
    auto adj = static_cast<SpinButton*>(_widget)->get_adjustment();;
    return adj->get_value();
}

int Scalar::getValueAsInt() const
{
    g_assert(_widget != nullptr);
    auto adj = static_cast<SpinButton*>(_widget)->get_adjustment();;
    return (int)adj->get_value();
}


void Scalar::setDigits(unsigned digits)
{
    g_assert(_widget != nullptr);
    static_cast<SpinButton*>(_widget)->set_digits(digits);
}

void Scalar::setIncrements(double step, double /*page*/)
{
    g_assert(_widget != nullptr);
    auto adj = static_cast<SpinButton*>(_widget)->get_adjustment();;
    adj->set_step_increment(step);
}

void Scalar::setRange(double min, double max)
{
    g_assert(_widget != nullptr);
    auto adj = static_cast<SpinButton*>(_widget)->get_adjustment();
   // adj->set_lower(min);
   // adj->set_upper(max);
}

void Scalar::setValue(double value, bool setProg)
{
    g_assert(_widget != nullptr);
    if (setProg) {
        setProgrammatically = true; // callback is supposed to reset back, if it cares
    }
    auto adj = static_cast<SpinButton*>(_widget)->get_adjustment();;
    adj->set_value(value);
}

void Scalar::setWidthChars(unsigned chars)
{
    g_assert(_widget != nullptr);
    /* auto adj = static_cast<SpinButton*>(_widget)->get_adjustment();;
    adj->set_width_chars(chars); */
}

void Scalar::update()
{
    g_assert(_widget != nullptr);
    static_cast<SpinButton*>(_widget)->update();
}

void Scalar::addSlider()
{
    auto scale = new Gtk::Scale(static_cast<SpinButton*>(_widget)->get_adjustment());
    scale->set_draw_value(false);
    pack_start(*manage (scale));
}

Glib::SignalProxy0<void> Scalar::signal_value_changed()
{
    auto adj = static_cast<SpinButton*>(_widget)->get_adjustment();;
    return adj->signal_value_changed();
}

Glib::SignalProxy1<bool, GdkEventButton*> Scalar::signal_button_release_event()
{
    return static_cast<SpinButton*>(_widget)->signal_button_release_event();
}

void Scalar::hide_label() {
    if (auto label = const_cast<Gtk::Label*>(getLabel())) {
        label->hide();
        label->set_no_show_all();
        label->set_hexpand(true);
    }
    if (_widget) {
        remove(*_widget);
        _widget->set_hexpand();
        this->pack_end(*_widget);
    }
}

} // namespace Ink2
} // namespace Widget
} // namespace UI
} // namespace Inkscape

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
