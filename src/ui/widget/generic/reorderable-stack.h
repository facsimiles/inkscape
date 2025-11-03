// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Build a stack of buttons who's order in the stack is the main value.
 *//*
 * Copyright (C) 2025 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_UI_WIDGET_REORDERABLE_STACK_H
#define SEEN_UI_WIDGET_REORDERABLE_STACK_H

#include <vector>
#include <ranges>

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/eventcontrollermotion.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/image.h>
#include <gtkmm/label.h>
//#include <gtkmm/listbox.h>

#include "../tab-strip.h"

#include "ui/pack.h"

namespace Inkscape::UI::Widget {

class ReorderableStack : public TabStrip //Gtk::ListBox
{
public:
    ReorderableStack();

    void add_option(std::string const &label, std::string const &icon, std::string const &tooltip, int value);

    void setValues(std::vector<int> const &values);
    std::vector<int> getValues() const;

    sigc::signal<void ()> signal_values_changed() { return _signal_values_changed; }

private:
    sigc::signal<void ()> _signal_values_changed;

    std::vector<std::pair<Gtk::Widget *, int>> _rows;
};

} // namespace Inkscape::UI::Widget

#endif /* !SEEN_UI_WIDGET_REORDERABLE_STACK_H */

/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim:filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99:
