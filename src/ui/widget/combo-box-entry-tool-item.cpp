// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * A class derived from Gtk::ToolItem that wraps a GtkComboBoxEntry.
 * Features:
 *   Setting GtkEntryBox width in characters.
 *   Passing a function for formatting cells.
 *   Displaying a warning if entry text isn't in list.
 *   Check comma separated values in text against list. (Useful for font-family fallbacks.)
 *   Setting names for GtkComboBoxEntry and GtkEntry (actionName_combobox, actionName_entry)
 *     to allow setting resources.
 *
 * Author(s):
 *   Tavmjong Bah
 *   Jon A. Cruz <jon@joncruz.org>
 *
 * Copyright (C) 2010 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

/*
 * We must provide for both a toolbar item and a menu item.
 * As we don't know which widgets are used (or even constructed),
 * we must keep track of things like active entry ourselves.
 */

#include "combo-box-entry-tool-item.h"

#include <cassert>
#include <cstring>
#include <utility>
#include <gdk/gdkkeysyms.h>
#include <glibmm/main.h>
#include <glibmm/regex.h>
#include <gdkmm/display.h>
#include <gtkmm/combobox.h>
#include <gtkmm/entry.h>
#include <gtkmm/eventcontrollerkey.h>

namespace Inkscape::UI::Widget {

ComboBoxEntryToolItem::ComboBoxEntryToolItem(Glib::ustring name,
                                             Glib::ustring label,
                                             Glib::ustring tooltip,
                                             Glib::RefPtr<Gtk::TreeModel> model,
                                             int           entry_width,
                                             int           extra_width,
                                             CellDataFunc  cell_data_func,
                                             SeparatorFunc separator_func)
    : _label(std::move(label))
    , _tooltip(std::move(tooltip))
    , _model(std::move(model))
    , _combobox(true)
    , _entry_width(entry_width)
    , _extra_width(extra_width)
    , _cell_data_func(std::move(cell_data_func))
{
    // Optionally add separator function...
    if (separator_func) {
        _combobox.set_row_separator_func(separator_func);
    }

    // Optionally widen the combobox width... which widens the drop-down list in list mode.
    if (_extra_width > 0) {
        Gtk::Requisition req, ignore;
        _combobox.get_preferred_size(req, ignore);
        _combobox.set_size_request(req.get_width() + _extra_width, -1);
    }

    // Entry is the first child of the box parented by the combo box.
    auto const box = dynamic_cast<Gtk::Box *>(_combobox.get_first_child());
    _entry = dynamic_cast<Gtk::Entry *>(box->get_first_child());
    if (_entry) {
        // Add pop-up entry completion if required
        if (_popup) {
            popup_enable();
        }
    }
}

void ComboBoxEntryToolItem::set_extra_width(int extra_width)
{
    // Clamp to limits
    _extra_width = std::clamp(extra_width, -1, 500);

    Gtk::Requisition req, ignore;
    _combobox.get_preferred_size(req, ignore);
    _combobox.set_size_request(req.get_width() + _extra_width, -1);
}

void ComboBoxEntryToolItem::popup_enable()
{
    _popup = true;

    // Widget may not have been created....
    if (!_entry) {
        return;
    }

    // Check we don't already have a GtkEntryCompletion
    if (_entry_completion) {
        return;
    }

    _entry_completion = Gtk::EntryCompletion::create();

    _entry->set_completion(_entry_completion);
    _entry_completion->set_model(_model);
    _entry_completion->set_text_column(0);
    _entry_completion->set_popup_completion(true);
    _entry_completion->set_inline_completion(false);
    _entry_completion->set_inline_selection(true);

    _entry_completion->signal_match_selected().connect(sigc::mem_fun(*this, &ComboBoxEntryToolItem::match_selected_cb), false);
}

void ComboBoxEntryToolItem::popup_disable()
{
    _popup = false;
    _entry_completion.reset();
}

} // namespace Inkscape::UI::Widget

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
