#ifndef INKSCAPE_UI_DIALOG_NEW_DOCUMENT_H
#define INKSCAPE_UI_DIALOG_NEW_DOCUMENT_H

#include <glibmm/refptr.h>
#include <gtkmm/window.h>
#include <gtkmm.h>
#include <gtkmm/builder.h>

class NewDocumentDialog : public Gtk::Window {
    public:
        NewDocumentDialog();
        ~NewDocumentDialog();
    private:
        Glib::RefPtr<Gtk::Builder> _builder;
}

#endif
