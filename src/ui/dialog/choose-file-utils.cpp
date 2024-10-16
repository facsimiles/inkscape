// SPDX-License-Identifier: GPL-2.0-or-later

#include "choose-file-utils.h"

#include <iostream>

#include <giomm/liststore.h>
#include <glibmm/fileutils.h>  // Glib::FileTest
#include <glibmm/i18n.h>
#include <glibmm/miscutils.h>  // Glib::get_home_dir(), etc.
#include <gtkmm/filefilter.h>

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

Glib::RefPtr<Gio::ListStore<Gtk::FileFilter>>
create_open_filters() {

  auto filters = Gio::ListStore<Gtk::FileFilter>::create();

  auto allfiles = Gtk::FileFilter::create();
  allfiles->set_name(_("All Files"));
  allfiles->add_pattern("*");
  filters->append(allfiles);

  auto inkscape = Gtk::FileFilter::create();
  inkscape->set_name(_("All Inkscape Files"));
  filters->append(inkscape);

  auto images = Gtk::FileFilter::create();
  images->set_name(_("Images"));
  filters->append(images);

  auto bitmaps = Gtk::FileFilter::create();
  bitmaps->set_name(_("Bitmaps"));
  filters->append(bitmaps);

  auto vectors = Gtk::FileFilter::create();
  vectors->set_name(_("Vectors"));
  filters->append(vectors);

  // Patterns added dynamically below based on which files are supported by input extensions.
  Inkscape::Extension::DB::InputList extension_list;
  Inkscape::Extension::db.get_input_list(extension_list);

  for (auto imod : extension_list) {

    gchar const * extension = imod->get_extension();
    if (extension[0]) {
      extension = extension + 1; // extension begins with '.', we need it without!
    }

    // TODO: Evaluate add_mime_type() instead of add_suffix(). This might allow opening
    // files with wrong extension.

    // Add filter for this extension.
    auto filter = Gtk::FileFilter::create();
    filter->set_name(imod->get_filetypename(true));
    filter->add_suffix(extension); // Both upper and lower cases.
    filters->append(filter);

    inkscape->add_suffix(extension);

    if (strncmp("image", imod->get_mimetype(), 5) == 0) {
      images->add_suffix(extension);
    }

    // I don't know of any other way to define "bitmap" formats other than by listing them
    // clang-format off
    if (strncmp("image/png",              imod->get_mimetype(),  9) == 0 ||
	strncmp("image/jpeg",             imod->get_mimetype(), 10) == 0 ||
	strncmp("image/gif",              imod->get_mimetype(),  9) == 0 ||
	strncmp("image/x-icon",           imod->get_mimetype(), 12) == 0 ||
	strncmp("image/x-navi-animation", imod->get_mimetype(), 22) == 0 ||
	strncmp("image/x-cmu-raster",     imod->get_mimetype(), 18) == 0 ||
	strncmp("image/x-xpixmap",        imod->get_mimetype(), 15) == 0 ||
	strncmp("image/bmp",              imod->get_mimetype(),  9) == 0 ||
	strncmp("image/vnd.wap.wbmp",     imod->get_mimetype(), 18) == 0 ||
	strncmp("image/tiff",             imod->get_mimetype(), 10) == 0 ||
	strncmp("image/x-xbitmap",        imod->get_mimetype(), 15) == 0 ||
	strncmp("image/x-tga",            imod->get_mimetype(), 11) == 0 ||
	strncmp("image/x-pcx",            imod->get_mimetype(), 11) == 0) {
      bitmaps->add_suffix(extension);
    } else {
      vectors->add_suffix(extension);
    }
    // clang-format on
  } // Loop over extension_list

  return filters;
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
