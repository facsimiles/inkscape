// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Implementation of the file dialog interfaces defined in filedialog.h.
 */
/* Authors:
 *   Bob Jamison
 *   Joel Holdsworth
 *   Others from The Inkscape Organization
 *
 * Copyright (C) 2004-2007 Bob Jamison
 * Copyright (C) 2006 Johan Engelen <johan@shouraizou.nl>
 * Copyright (C) 2007-2008 Joel Holdsworth
 * Copyright (C) 2004-2008 The Inkscape Organization
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "filedialog.h"

#include <gtkmm.h>  // Glib::get_home_dir()
#include <glibmm/convert.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "filedialogimpl-gtkmm.h"

#include "preferences.h"
#include "ui/dialog-events.h"
#include "extension/output.h"


namespace Inkscape::UI::Dialog {

/*#########################################################################
  ### U T I L I T Y
  #########################################################################*/

bool hasSuffix(const Glib::ustring &str, const Glib::ustring &ext)
{
    int strLen = str.length();
    int extLen = ext.length();
    if (extLen > strLen)
        return false;
    int strpos = strLen-1;
    for (int extpos = extLen-1 ; extpos>=0 ; extpos--, strpos--) {
        Glib::ustring::value_type ch = str[strpos];
        if (ch != ext[extpos]) {
            if ( ((ch & 0xff80) != 0) ||
                 static_cast<Glib::ustring::value_type>( g_ascii_tolower( static_cast<gchar>(0x07f & ch) ) ) != ext[extpos] )
            {
                return false;
            }
        }
    }
    return true;
}

bool isValidImageFile(const Glib::ustring &fileName)
{
    std::vector<Gdk::PixbufFormat>formats = Gdk::Pixbuf::get_formats(); // Returns Glib::ustrings!
    for (auto format : formats)
    {
        std::vector<Glib::ustring>extensions = format.get_extensions();
        for (auto ext : extensions)
        {
            if (hasSuffix(fileName, ext))
                return true;
        }
    }
    return false;
}


//########################################################################
//# F I L E    S A V E
//########################################################################

/**
 * Public factory method.  Used in file.cpp
 */
std::unique_ptr<FileSaveDialog> FileSaveDialog::create(Gtk::Window& parentWindow,
                                                       std::string const &path,
                                                       FileDialogType fileTypes,
                                                       char const *title,
                                                       Glib::ustring const &default_key,
                                                       char const *docTitle,
                                                       Inkscape::Extension::FileSaveMethod save_method)
{
    return std::make_unique<FileSaveDialogImplGtk>(parentWindow, path, fileTypes, title, default_key, docTitle, save_method);
}

// Used in FileSaveDialogImplGtk to update displayed filename (thus utf8).
void FileSaveDialog::appendExtension(Glib::ustring& filename_utf8, Inkscape::Extension::Output* outputExtension)
{
    if (!outputExtension) {
        return;
    }

    bool appendExtension = true;
    Glib::ustring::size_type pos = filename_utf8.rfind('.');
    if ( pos != Glib::ustring::npos ) {
        Glib::ustring trail = filename_utf8.substr( pos );
        Glib::ustring foldedTrail = trail.casefold();
        if ( (trail == ".")
             | (foldedTrail != Glib::ustring( outputExtension->get_extension() ).casefold()
                && ( knownExtensions.find(foldedTrail) != knownExtensions.end() ) ) ) {
            filename_utf8 = filename_utf8.erase( pos );
        } else {
            appendExtension = false;
        }
    }

    if (appendExtension) {
        filename_utf8 = filename_utf8 + outputExtension->get_extension();
    }
}

} //namespace Inkscape::UI::Dialog

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
