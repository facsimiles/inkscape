// SPDX-License-Identifier: GPL-2.0-or-later

#include "choose-file-utils.h"

#include <iostream>

#include <glibmm/fileutils.h>  // Glib::FileTest
#include <glibmm/miscutils.h>  // Glib::get_home_dir(), etc.

#include "preferences.h"

#include "extension/db.h"
#include "extension/input.h"

namespace Inkscape::UI::Dialog {

// TODO: Should we always try to use document directory if no path in preferences?
void get_start_directory(std::string &start_path, Glib::ustring const &prefs_path, bool try_document_dir)
{
    // Get the current directory for finding files.
    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    std::string attr = prefs->getString(prefs_path); // Glib::ustring -> std::string
    if (!attr.empty()) {
        start_path = attr;
    }

    // Test if the path directory exists.
    if (!Glib::file_test(start_path, Glib::FileTest::EXISTS)) {
        std::cerr << "get_start_directory: directory does not exist: " << start_path << std::endl;
        start_path = "";
    }

    // If no start path, try document directory.
    if (start_path.empty() && try_document_dir) {
        start_path = Glib::get_user_special_dir(Glib::UserDirectory::DOCUMENTS);
    }

    // If no start path, default to home directory.
    if (start_path.empty()) {
        start_path = Glib::get_home_dir();
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
// vim:filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4:fileencoding=utf-8:textwidth=99:
