// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Entry widget for typing color value in css form
 *//*
 * Authors:
 *   Tomasz Boczkowski <penginsbacon@gmail.com>
 *
 * Copyright (C) 2014 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */
#include <glibmm.h>
#include <glibmm/i18n.h>
#include <iomanip>
#include <iostream>

#include "color-entry.h"

#include "colors/printer.h"
#include "colors/spaces/base.h"
// #include "util-string/ustring-format.h"

namespace Inkscape::UI::Widget {

ColorEntry::ColorEntry(std::shared_ptr<Colors::ColorSet> colors)
    : _colors(std::move(colors))
    , _updating(false)
    , _updatingrgba(false)
    , _prevpos(0)
{
    set_name("ColorEntry");

    _color_changed_connection = _colors->signal_changed.connect(sigc::mem_fun(*this, &ColorEntry::_onColorChanged));
    signal_activate().connect(sigc::mem_fun(*this, &ColorEntry::_onColorChanged));
    get_buffer()->signal_inserted_text().connect(sigc::mem_fun(*this, &ColorEntry::_inputCheck));
    _onColorChanged();

    // add extra character for pasting a hash, '#11223344'
    set_max_length(9);
    set_max_width_chars(9);
    set_width_chars(8);
    set_tooltip_text(_("Hexadecimal RGB value of the color"));
}

ColorEntry::~ColorEntry()
{
    _color_changed_connection.disconnect();
}

void ColorEntry::_inputCheck(guint pos, const gchar * /*chars*/, guint n_chars)
{
    // remember position of last character, so we can remove it.
    // we only overflow by 1 character at most.
    _prevpos = pos + n_chars - 1;
}

void ColorEntry::on_changed()
{
    if (_updating || _updatingrgba) {
        return;
    }

    auto new_color = Colors::Color::parse(get_text());

    _updatingrgba = true;
    if (new_color) {
        if (auto color = _colors->get()) {
            new_color->setOpacity(color->getOpacity());
        }
        _colors->setAll(*new_color);
    }
    _updatingrgba = false;
}


void ColorEntry::_onColorChanged()
{
    if (_updatingrgba) {
        return;
    }

    if (_colors->isEmpty()) {
        set_text(_("N/A"));
        return;
    }

    auto color = _colors->getAverage().converted(Colors::Space::Type::RGB);
    if (color.has_value()) {
        if (color->getSpace()->outOfGamut(color->getValues())) {
            // out of sRGB gamut warning
            auto r = color->get(0);
            auto g = color->get(1);
            auto b = color->get(2);
            auto rgb = Colors::CssLegacyPrinter(3, "rgb", false);
            // high precision, so we show values just barely above/below limits
            rgb.precision(5);
            rgb << r << g << b;
            _signal_out_of_gamut.emit(Glib::ustring::compose(_("Color %1 is out of sRGB gamut.\nIt has been clipped to sRGB boundary."), static_cast<std::string>(rgb).c_str()));
            _warning = true;
        }
        else if (_warning) {
            // clear warning
            _warning = false;
            _signal_out_of_gamut.emit({});
        }
    }
    auto text = color.has_value() ? color->toString(false) : "?";

    if (get_text().raw() != text) {
        _updating = true;
        set_text(text);
        _updating = false;
    }
}

} // namespace

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
