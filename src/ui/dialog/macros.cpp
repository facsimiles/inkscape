// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Macros dialog - implementation.
 * Macros group of action that can be repeated many times
 */
/* Author:
 *   Abhay Raj Singh <abhayonlyone@gmail.com>
 *
 * Copyright (C) 2020 Authors
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "macros.h"

#include <glib/gi18n.h>
#include <gtkmm/box.h>
#include <gtkmm/builder.h>
#include <gtkmm/button.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/dialog.h>
#include <gtkmm/enums.h>
#include <gtkmm/hvbox.h>
#include <iostream>
#include <sigc++/functors/mem_fun.h>

#include "io/resource.h"
#include "ui/widget/panel.h"
#include "verbs.h"

namespace Inkscape {
namespace UI {
namespace Dialog {

Macros::Macros()
    : UI::Widget::Panel("/dialogs/macros", SP_VERB_DIALOG_MACROS)
/* , _MacroCreate(nullptr) */
/* , _MacroDelete(nullptr) */
/* , _MacroImport(nullptr) */
/* , _MacroExport(nullptr) */
/* , _MacroRecord(nullptr) */
/* , _MacroPlay(nullptr) */
/* , _MacroEdit(nullptr) */
/* , _MacroBase(nullptr) */
{
    std::string gladefile = IO::Resource::get_filename_string(Inkscape::IO::Resource::UIS, "dialog-macros.glade");
    Glib::RefPtr<Gtk::Builder> builder;
    try {
        builder = Gtk::Builder::create_from_file(gladefile);
    } catch (const Glib::Error &ex) {
        g_warning("GtkBuilder file loading failed for Macros dialog");
        return;
    }

    // Linking UI
    builder->get_widget("MacrosBase", _MacrosBase);

    builder->get_widget("MacrosCreate", _MacrosCreate);
    builder->get_widget("MacrosDelete", _MacrosDelete);
    builder->get_widget("MacrosImport", _MacrosImport);
    builder->get_widget("MacrosExport", _MacrosExport);

    builder->get_widget("MacrosRecord", _MacrosRecord);
    builder->get_widget("MacrosPlay", _MacrosPlay);
    builder->get_widget("MacrosEdit", _MacrosEdit);

    builder->get_widget("record-icon", _record_button_icon);

    // Adding signals
    _MacrosCreate->signal_clicked().connect(sigc::mem_fun(*this, &Macros::on_macro_create));
    _MacrosDelete->signal_clicked().connect(sigc::mem_fun(*this, &Macros::on_macro_delete));
    _MacrosImport->signal_clicked().connect(sigc::mem_fun(*this, &Macros::on_macro_import));
    _MacrosExport->signal_clicked().connect(sigc::mem_fun(*this, &Macros::on_macro_export));

    _MacrosRecord->signal_clicked().connect(sigc::mem_fun(*this, &Macros::on_macro_record));
    _MacrosPlay->signal_clicked().connect(sigc::mem_fun(*this, &Macros::on_macro_play));
    _MacrosEdit->signal_clicked().connect(sigc::mem_fun(*this, &Macros::on_macro_edit));

    // TODO: Initialize Marcos tree (actual macros)

    _setContents(_MacrosBase);
    show_all_children();
}

Macros::~Macros() {}

// Overrides
/* void Macros::_apply() */
/* { */
/*     return; */
/* } */

/* void Macros::setDesktop(SPDesktop *desktop) */
/* { */
/*     Panel::setDesktop(desktop); */
/*     return; */
/* } */

// Listeners
void Macros::on_macro_create()
{
    Gtk::Dialog dialog;
    Gtk::VBox box;
    Gtk::Entry name_entry;
    Gtk::ComboBoxText group_combo(true);
    Gtk::Label name_label(_("Macro name"));
    Gtk::Label group_label(_("Group"));

    // Test Data
    group_combo.append("111");
    group_combo.append("222");
    group_combo.append("333");
    // Test Data

    dialog.set_title(_("Create new Macro"));

    name_label.set_alignment(0);
    group_label.set_alignment(0);

    box.pack_start(name_label);
    box.pack_start(name_entry);
    box.pack_start(group_label);
    box.pack_start(group_combo);

    box.set_valign(Gtk::ALIGN_START);

    dialog.get_content_area()->pack_start(box);

    dialog.add_button(_("Cancel"), Gtk::RESPONSE_CANCEL);
    dialog.add_button(_("Create"), Gtk::RESPONSE_OK);

    dialog.show_all();

    int result = dialog.run();

    if (result == Gtk::RESPONSE_OK) {
        Glib::ustring macro_name = name_label.get_text();
        Glib::ustring macro_group = group_label.get_text();
    }
}

void Macros::on_macro_delete()
{
    std::cout << "Macro delete not implemented" << std::endl;
}

void Macros::on_macro_import()
{
    std::cout << "Macro import not implemented" << std::endl;
}

void Macros::on_macro_export()
{
    std::cout << "Macro export not implemented" << std::endl;
}

void Macros::on_macro_record()
{
    // Add recording logic
    if (_is_recording) {
        // In recording, stop and change button to Record
        _MacrosRecord->set_tooltip_text(_("Record"));
        _record_button_icon->set_from_icon_name("media-record", Gtk::ICON_SIZE_BUTTON);

        _is_recording = false;
        return;
    }

    // Not recording , start recording and change button to Record
    _MacrosRecord->set_tooltip_text(_("Stop Recording"));
    _record_button_icon->set_from_icon_name("media-playback-stop", Gtk::ICON_SIZE_BUTTON);
    _is_recording = true;
}

void Macros::on_macro_play()
{
    std::cout << "Macro play not implemented" << std::endl;
}

void Macros::on_macro_edit()
{
    std::cout << "Macro edit not implemented" << std::endl;
}

} // namespace Dialog
} // namespace UI
} // namespace Inkscape
