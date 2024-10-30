// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * The job of this window is to ask the user for document size when creating a new document.
 */

#ifndef INKSCAPE_UI_DIALOG_NEW_DOCUMENT_H
#define INKSCAPE_UI_DIALOG_NEW_DOCUMENT_H

#include <glibmm/refptr.h>
#include <gtkmm/window.h>
#include <gtkmm.h>
#include <gtkmm/builder.h>

#include "ui/builder-utils.h"

namespace Inkscape::UI {

class NewDocumentDialog : public Gtk::Window {
    public:
        NewDocumentDialog();
        ~NewDocumentDialog();
    private:
        Glib::RefPtr<Gtk::Builder> _builder;
};

} // namespace Inkscape::UI

#endif // INKSCAPE_UI_DIALOG_NEW_DOCUMENT_H
