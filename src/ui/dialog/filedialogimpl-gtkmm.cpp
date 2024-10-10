// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Implementation of the file dialog interfaces defined in filedialogimpl.h.
 */
/* Authors:
 *   Bob Jamison
 *   Joel Holdsworth
 *   Bruno Dilly
 *   Others from The Inkscape Organization
 *   Abhishek Sharma
 *
 * Copyright (C) 2004-2007 Bob Jamison
 * Copyright (C) 2006 Johan Engelen <johan@shouraizou.nl>
 * Copyright (C) 2007-2008 Joel Holdsworth
 * Copyright (C) 2004-2007 The Inkscape Organization
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "filedialogimpl-gtkmm.h"

#include <iostream>

#include <glibmm/convert.h>
#include <glibmm/fileutils.h>
#include <glibmm/i18n.h>
#include <glibmm/miscutils.h>
#include <glibmm/stringutils.h>
#include <glibmm/main.h>
#include <giomm/liststore.h>
#include <gtkmm/error.h>

#include "extension/db.h"
#include "extension/input.h"
#include "extension/output.h"

namespace Inkscape::UI::Dialog {

template <typename F>
static void pump_until(F const &f)
{
    auto main_context = Glib::MainContext::get_default();
    while (!f()) {
        main_context->iteration(true);
    }
}

/*#########################################################################
### F I L E     D I A L O G    B A S E    C L A S S
#########################################################################*/

FileDialogBaseGtk::FileDialogBaseGtk(Gtk::Window &parentWindow, Glib::ustring const &title,
                                     Action dialogType,
                                     FileDialogType type)
    : _dialog{Gtk::FileDialog::create()}
    , _dialogtype{dialogType}
    , _parent_window{parentWindow}
    , _dialogType{type}
    , _filters{Gio::ListStore<Gtk::FileFilter>::create()}
{
    _dialog->set_title(title);
    _dialog->set_filters(_filters);
    _dialog->set_modal();
}

FileDialogBaseGtk::~FileDialogBaseGtk() = default;

Glib::RefPtr<Gtk::FileFilter> FileDialogBaseGtk::addFilter(Glib::ustring const &name, Glib::ustring ext,
                                                           Inkscape::Extension::Extension *extension)
{
    auto filter = Gtk::FileFilter::create();
    filter->set_name(name);
    _filters->append(filter);

    if (!ext.empty()) {
        filter->add_pattern(extToPattern(ext));
    }

    filterExtensionMap[filter] = extension;
    extensionFilterMap[extension] = filter;

    return filter;
}

// Replace this with add_suffix in Gtk4
Glib::ustring FileDialogBaseGtk::extToPattern(Glib::ustring const &extension) const
{
    Glib::ustring pattern = "*";
    for (auto ch : extension) {
        if (Glib::Unicode::isalpha(ch)) {
            pattern += '[';
            pattern += Glib::Unicode::toupper(ch);
            pattern += Glib::Unicode::tolower(ch);
            pattern += ']';
        } else {
            pattern += ch;
        }
    }
    return pattern;
}

/*#########################################################################
### F I L E    O P E N
#########################################################################*/

/**
 * Constructor.  Not called directly.  Use the factory.
 */
FileOpenDialogImplGtk::FileOpenDialogImplGtk(Gtk::Window &parentWindow, std::string const &dir,
                                             FileDialogType fileTypes, Glib::ustring const &title)
    : FileDialogBaseGtk(parentWindow, title, Action::OPEN, fileTypes)
{
    if (_dialogType == EXE_TYPES) {
        /* One file at a time */
        setSelectMultiple(false);
    } else {
        /* And also Multiple Files */
        setSelectMultiple(true);
    }

    /* Set our dialog type (open, import, etc...)*/
    _dialogType = fileTypes;

    /* Set the pwd and/or the filename */
    if (!dir.empty()) {
        auto udir = dir;
        // leaving a trailing backslash on the directory name leads to the infamous
        // double-directory bug on win32

        if (!udir.empty() && udir.back() == '\\') {
            udir.pop_back();
        }

        auto file = Gio::File::create_for_path(udir);
        if (_dialogType == EXE_TYPES) {
            _dialog->set_initial_file(file);
        } else {
            _dialog->set_initial_folder(file);
        }
    }

    // Add the file types menu.
    createFilterMenu();
}

void FileOpenDialogImplGtk::createFilterMenu()
{
    if (_dialogType == CUSTOM_TYPE) {
        return;
    }

    addFilter(_("All Files"), "*");

    if (_dialogType != EXE_TYPES) {
        auto allInkscapeFilter = addFilter(_("All Inkscape Files"));
        auto allImageFilter = addFilter(_("All Images"));
        auto allVectorFilter = addFilter(_("All Vectors"));
        auto allBitmapFilter = addFilter(_("All Bitmaps"));

        // patterns added dynamically below
        Inkscape::Extension::DB::InputList extension_list;
        Inkscape::Extension::db.get_input_list(extension_list);

        for (auto imod : extension_list) {
            addFilter(imod->get_filetypename(true), imod->get_extension(), imod);

            auto upattern = extToPattern(imod->get_extension());
            allInkscapeFilter->add_pattern(upattern);
            if (strncmp("image", imod->get_mimetype(), 5) == 0)
                allImageFilter->add_pattern(upattern);

            // I don't know of any other way to define "bitmap" formats other than by listing them
            if (strncmp("image/png", imod->get_mimetype(), 9) == 0 ||
                strncmp("image/jpeg", imod->get_mimetype(), 10) == 0 ||
                strncmp("image/gif", imod->get_mimetype(), 9) == 0 ||
                strncmp("image/x-icon", imod->get_mimetype(), 12) == 0 ||
                strncmp("image/x-navi-animation", imod->get_mimetype(), 22) == 0 ||
                strncmp("image/x-cmu-raster", imod->get_mimetype(), 18) == 0 ||
                strncmp("image/x-xpixmap", imod->get_mimetype(), 15) == 0 ||
                strncmp("image/bmp", imod->get_mimetype(), 9) == 0 ||
                strncmp("image/vnd.wap.wbmp", imod->get_mimetype(), 18) == 0 ||
                strncmp("image/tiff", imod->get_mimetype(), 10) == 0 ||
                strncmp("image/x-xbitmap", imod->get_mimetype(), 15) == 0 ||
                strncmp("image/x-tga", imod->get_mimetype(), 11) == 0 ||
                strncmp("image/x-pcx", imod->get_mimetype(), 11) == 0)
            {
                allBitmapFilter->add_pattern(upattern);
            } else {
                allVectorFilter->add_pattern(upattern);
            }
        }
    }
}

/**
 * Show this dialog modally. Return true if user hits [OK]
 */
bool FileOpenDialogImplGtk::show()
{
    try {

        if (!_select_multiple) {
            if (_dialogtype == Action::OPEN) {
                Glib::RefPtr<Gio::AsyncResult> result;
                _dialog->open(_parent_window, [&] (auto res) { result = res; });
                pump_until([&] { return !!result; });
                _file = _dialog->open_finish(result);
            } else if (_dialogtype == Action::SELECT_FOLDER) {
                Glib::RefPtr<Gio::AsyncResult> result;
                _dialog->select_folder(_parent_window, [&] (auto res) { result = res; });
                pump_until([&] { return !!result; });
                _file = _dialog->select_folder_finish(result);
            }
        } else {
            if (_dialogtype == Action::OPEN) {
                Glib::RefPtr<Gio::AsyncResult> result;
                _dialog->open_multiple(_parent_window, [&] (auto res) { result = res; });
                pump_until([&] { return !!result; });
                _files = _dialog->open_multiple_finish(result);
            } else if (_dialogtype == Action::SELECT_FOLDER) {
                Glib::RefPtr<Gio::AsyncResult> result;
                _dialog->select_multiple_folders(_parent_window, [&] (auto res) { result = res; });
                pump_until([&] { return !!result; });
                _files = _dialog->select_multiple_folders_finish(result);
            }
        }

    } catch (Gtk::DialogError const &) {
        return false;
    }

    return true;
}

//########################################################################
//# F I L E    S A V E
//########################################################################

/**
 * Constructor
 */
FileSaveDialogImplGtk::FileSaveDialogImplGtk(Gtk::Window &parentWindow, std::string const &dir,
                                             FileDialogType fileTypes, Glib::ustring const &title,
                                             Glib::ustring const & /*default_key*/, char const *docTitle,
                                             Inkscape::Extension::FileSaveMethod save_method)
    : FileDialogBaseGtk(parentWindow, title, Action::SAVE, fileTypes)
    , save_method(save_method)
{
    if (docTitle) {
        FileSaveDialog::myDocTitle = docTitle;
    }

    // ===== Filters =====

    if (_dialogType != CUSTOM_TYPE) {
        createFilterMenu();
    }

    // ===== Initial Value =====

    // Set the directory or filename. Do this last, after dialog is completely set up.
    if (!dir.empty()) {
        std::string udir = dir;
        // Leaving a trailing backslash on the directory name leads to the infamous
        // double-directory bug on win32.
        if (!udir.empty() && udir.back() == '\\') {
            udir.pop_back();
        }

        auto file = Gio::File::create_for_path(udir);
        auto display_name = Glib::filename_to_utf8(file->get_basename());
        Gio::FileType type = file->query_file_type();
        switch (type) {
            case Gio::FileType::UNKNOWN:
            case Gio::FileType::REGULAR:
                // The extension set here is over-written when called by sp_file_save_dialog().
                _dialog->set_initial_file(file);
                break;
            case Gio::FileType::DIRECTORY:
                _dialog->set_initial_folder(file);
                break;
            default:
                std::cerr << "FileDialogImplGtk: Unknown file type: " << (int)type << std::endl;
                break;
        }
    }
}

/**
 * Show this dialog modally.  Return true if user hits [OK]
 */
bool FileSaveDialogImplGtk::show()
{
    try {
        Glib::RefPtr<Gio::AsyncResult> result;
        _dialog->save(_parent_window, [&] (auto res) { result = res; });
        pump_until([&] { return !!result; });
        _file = _dialog->save_finish(result);
    } catch (Gtk::DialogError const &) {
        return false;
    }

    auto extension = getExtension();
    Inkscape::Extension::store_file_extension_in_prefs(extension ? extension->get_id() : "", save_method);
    return true;
}

// Given filename, find module for saving. If found, update all.
bool FileSaveDialogImplGtk::setExtension(Glib::ustring const &filename_utf8)
{
    // Find module
    Glib::ustring filename_folded = filename_utf8.casefold();
    Inkscape::Extension::Extension *key = nullptr;
    for (auto const &iter : knownExtensions) {
        auto ext = Glib::ustring(iter.second->get_extension()).casefold();
        if (Glib::str_has_suffix(filename_folded, ext)) {
            key = iter.second;
        }
    }

    if (!key) {
        // This happens when saving shortcuts.
        return false;
    }

    // Update all.
    setExtension(key);
    return true;
}

// Given module, set filter and filename (if required).
// If module is null, try to find module from current name.
void FileSaveDialogImplGtk::setExtension(Inkscape::Extension::Extension *key)
{
    if (!key) {
        // Try to use filename.
        auto filename_utf8 = Glib::filename_to_utf8(_file->get_path());
        setExtension(filename_utf8);
    }

    // Save module.
    FileDialog::setExtension(key);

    // Update filter.
    _dialog->set_default_filter(extensionFilterMap[key]);
}

void FileSaveDialogImplGtk::createFilterMenu()
{
    Inkscape::Extension::DB::OutputList extension_list;
    Inkscape::Extension::db.get_output_list(extension_list);
    knownExtensions.clear();

    addFilter(_("Guess from extension"), "*"); // No output module!

    for (auto omod : extension_list) {
        // Export types are either exported vector types, or any raster type.
        if (!omod->is_exported() && omod->is_raster() != (_dialogType == EXPORT_TYPES))
            continue;

        // This extension is limited to save copy only.
        if (omod->savecopy_only() && save_method != Inkscape::Extension::FILE_SAVE_METHOD_SAVE_COPY)
            continue;

        Glib::ustring extension = omod->get_extension();
        addFilter(omod->get_filetypename(true), extension, omod);
        knownExtensions.emplace(extension.casefold(), omod);
    }
}

} // namespace Inkscape::UI::Dialog

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
