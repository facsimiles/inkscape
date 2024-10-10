// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * @brief Implementation of the file dialog interfaces defined in filedialogimpl.h
 */
/* Authors:
 *   Bob Jamison
 *   Johan Engelen <johan@shouraizou.nl>
 *   Joel Holdsworth
 *   Bruno Dilly
 *   Others from The Inkscape Organization
 *
 * Copyright (C) 2004-2008 Authors
 * Copyright (C) 2004-2007 The Inkscape Organization
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_FILE_DIALOG_IMPL_GTKMM_H
#define SEEN_FILE_DIALOG_IMPL_GTKMM_H

#include <giomm/file.h>
#include <glibmm/ustring.h>
#include <giomm/liststore.h>
#include <gtkmm/filedialog.h>

#include "filedialog.h"

namespace Gtk {
class Entry;
class FileFilter;
class Window;
} // namespace Gtk

namespace Inkscape::UI::Dialog {

/*#########################################################################
### F I L E     D I A L O G    B A S E    C L A S S
#########################################################################*/

/**
 * This class is the base implementation for the others. This
 * reduces redundancies and bugs.
 */
class FileDialogBaseGtk
{
public:
    enum class Action
    {
        OPEN,
        SAVE,
        SELECT_FOLDER
    };

    FileDialogBaseGtk(Gtk::Window &parentWindow, Glib::ustring const &title,
                      Action dialogType, FileDialogType type);

    virtual ~FileDialogBaseGtk();

    /**
     * Add a Gtk filter to our specially controlled filter dropdown.
     */
    Glib::RefPtr<Gtk::FileFilter> addFilter(Glib::ustring const &name, Glib::ustring pattern = "",
                                            Inkscape::Extension::Extension *mod = nullptr);

    Glib::ustring extToPattern(Glib::ustring const &extension) const;

protected:
    Glib::RefPtr<Gtk::FileDialog> _dialog;
    Action _dialogtype;
    Gtk::Window &_parent_window;
    Glib::RefPtr<Gio::ListStore<Gtk::FileFilter>> _filters;

    /**
     * What type of 'open' are we? (open, import, place, etc)
     */
    FileDialogType _dialogType;

    /**
     * Maps extension <-> filter.
     */
    std::map<Glib::RefPtr<Gtk::FileFilter>, Inkscape::Extension::Extension *> filterExtensionMap;
    std::map<Inkscape::Extension::Extension *, Glib::RefPtr<Gtk::FileFilter>> extensionFilterMap;

    std::vector<Glib::RefPtr<Gio::File>> _files;
    Glib::RefPtr<Gio::File> _file;
};

/*#########################################################################
### F I L E    O P E N
#########################################################################*/

/**
 * Our implementation class for the FileOpenDialog interface.
 */
class FileOpenDialogImplGtk
    : public FileOpenDialog
    , public FileDialogBaseGtk
{
public:
    FileOpenDialogImplGtk(Gtk::Window& parentWindow,
                          std::string const &dir,
                          FileDialogType fileTypes,
                          Glib::ustring const &title);

    bool show() override;

    void setSelectMultiple(bool value) override { _select_multiple = value; }
    std::vector<Glib::RefPtr<Gio::File>> const &getFiles() override { return _files; }
    Glib::RefPtr<Gio::File> getFile() override { return _file; }

    Glib::RefPtr<Gio::File> getCurrentDirectory() override { return _file ? _file->get_parent() : nullptr; }

    void addFilterMenu(Glib::ustring const &name, Glib::ustring pattern = "",
                       Inkscape::Extension::Extension *mod = nullptr) override
    {
        addFilter(name, pattern, mod);
    }

private:
    bool _select_multiple = false;

    /**
     *  Create a filter menu for this type of dialog
     */
    void createFilterMenu();
};

//########################################################################
//# F I L E    S A V E
//########################################################################

/**
 * Our implementation of the FileSaveDialog interface.
 */
class FileSaveDialogImplGtk
    : public FileSaveDialog
    , public FileDialogBaseGtk
{
public:
    FileSaveDialogImplGtk(Gtk::Window &parentWindow,
                          std::string const &dir,
                          FileDialogType fileTypes,
                          Glib::ustring const &title,
                          Glib::ustring const &default_key,
                          char const *docTitle,
                          Inkscape::Extension::FileSaveMethod save_method);

    bool show() override;

    // One at a time.
    Glib::RefPtr<Gio::File> getFile() override { return _file; }

    void setCurrentName(Glib::ustring name) override { _dialog->set_initial_name(name); }
    Glib::RefPtr<Gio::File> getCurrentDirectory() override { return _file ? _file->get_parent() : nullptr; }

    // Sets module for saving, updating GUI if necessary.
    bool setExtension(Glib::ustring const &filename_utf8);
    void setExtension(Inkscape::Extension::Extension *key) override;

    void addFilterMenu(Glib::ustring const &name, Glib::ustring pattern = {},
                       Inkscape::Extension::Extension *mod = nullptr) override
    {
        addFilter(name, pattern, mod);
    }

private:
    /**
     * The file save method (essentially whether the dialog was invoked by "Save as ..." or "Save a
     * copy ..."), which is used to determine file extensions and save paths.
     */
    Inkscape::Extension::FileSaveMethod save_method;

    /**
     *  Create a filter menu for this type of dialog
     */
    void createFilterMenu();
};

} // namespace Inkscape::UI::Dialog

#endif // SEEN_FILE_DIALOG_IMPL_GTKMM_H

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
