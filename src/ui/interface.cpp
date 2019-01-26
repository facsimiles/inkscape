// SPDX-License-Identifier: GPL-2.0-or-later
/**
 * @file
 * Main UI stuff.
 */
/* Authors:
 *   Lauris Kaplinski <lauris@kaplinski.com>
 *   Frank Felfe <innerspace@iname.com>
 *   bulia byak <buliabyak@users.sf.net>
 *   Jon A. Cruz <jon@joncruz.org>
 *   Abhishek Sharma
 *   Kris De Gussem <Kris.DeGussem@gmail.com>
 *
 * Copyright (C) 2012 Kris De Gussem
 * Copyright (C) 2010 authors
 * Copyright (C) 1999-2005 authors
 * Copyright (C) 2004 David Turner
 * Copyright (C) 2001-2002 Ximian, Inc.
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "desktop.h"
#include "document.h"
#include "enums.h"
#include "file.h"
#include "inkscape.h"
#include "inkscape-window.h"
#include "preferences.h"
#include "shortcuts.h"

#include "extension/db.h"
#include "extension/effect.h"
#include "extension/find_extension_by_mime.h"
#include "extension/input.h"

#include "helper/action.h"

#include "io/sys.h"

#include "object/sp-namedview.h"
#include "object/sp-root.h"

#include "ui/dialog-events.h"
#include "ui/dialog/dialog-manager.h"
#include "ui/dialog/inkscape-preferences.h"
#include "ui/dialog/layer-properties.h"
#include "ui/interface.h"

#include "ui/view/svg-view-widget.h"

#include "widgets/desktop-widget.h"

static void sp_ui_import_one_file(char const *filename);
static void sp_ui_import_one_file_with_check(gpointer filename, gpointer unused);

void
sp_ui_new_view()
{
    SPDocument *document;
    SPViewWidget *dtw;

    document = SP_ACTIVE_DOCUMENT;
    if (!document) return;

    auto win = new InkscapeWindow(document);
}

void sp_ui_reload()
{

    Inkscape::Preferences *prefs = Inkscape::Preferences::get();
    prefs->setInt("/dialogs/preferences/page", PREFS_PAGE_UI_THEME);
    Inkscape::UI::Dialog::Dialog *prefs_dialog = SP_ACTIVE_DESKTOP->_dlg_mgr->getDialog("InkscapePreferences");
    if (prefs_dialog) {
        prefs_dialog->hide();
    }
    int window_geometry = prefs->getInt("/options/savewindowgeometry/value", PREFS_WINDOW_GEOMETRY_NONE);
    if (GtkSettings *settings = gtk_settings_get_default()) {
        Glib::ustring themeiconname = prefs->getString("/theme/iconTheme");
        if (themeiconname != "") {
            g_object_set(settings, "gtk-icon-theme-name", themeiconname.c_str(), NULL);
        }
    }
    prefs->setInt("/options/savewindowgeometry/value", PREFS_WINDOW_GEOMETRY_LAST);
    prefs->save();
    std::list<SPDesktop *> desktops;
    INKSCAPE.get_all_desktops(desktops);
    std::list<SPDesktop *>::iterator i = desktops.begin();
    while (i != desktops.end()) {
        SPDesktop *dt = *i;
        if (dt == nullptr) {
            ++i;
            continue;
        }
        dt->storeDesktopPosition();
        SPDocument *document;
        SPViewWidget *dtw;

        document = dt->getDocument();
        if (!document) {
            ++i;
            continue;
        }

        auto win = new InkscapeWindow(document);

        dt->destroyWidget();
        i++;
    }
    SP_ACTIVE_DESKTOP->_dlg_mgr->showDialog("InkscapePreferences");
    INKSCAPE.add_gtk_css();
    prefs->setInt("/options/savewindowgeometry/value", window_geometry);
}

void
sp_ui_close_view(GtkWidget */*widget*/)
{
    SPDesktop *dt = SP_ACTIVE_DESKTOP;

    if (dt == nullptr) {
        return;
    }

    if (dt->shutdown()) {
        return; // Shutdown operation has been canceled, so do nothing
    }

    // If closing the last document, open a new document so Inkscape doesn't quit.
    std::list<SPDesktop *> desktops;
    INKSCAPE.get_all_desktops(desktops);
    if (desktops.size() == 1) {
        Glib::ustring templateUri = sp_file_default_template_uri();
        SPDocument *doc = SPDocument::createNewDoc( templateUri.empty() ? nullptr : templateUri.c_str(), TRUE, true );
        // Set viewBox if it doesn't exist
        if (!doc->getRoot()->viewBox_set) {
            doc->setViewBox(Geom::Rect::from_xywh(0, 0, doc->getWidth().value(doc->getDisplayUnit()), doc->getHeight().value(doc->getDisplayUnit())));
        }
        dt->change_document(doc);
        sp_namedview_window_from_document(dt);
        sp_namedview_update_layers_from_document(dt);
        return;
    }

    // Shutdown can proceed; use the stored reference to the desktop here instead of the current SP_ACTIVE_DESKTOP,
    // because the user might have changed the focus in the meantime (see bug #381357 on Launchpad)
    dt->destroyWidget();
}


unsigned int
sp_ui_close_all()
{
    /* Iterate through all the windows, destroying each in the order they
       become active */
    while (SP_ACTIVE_DESKTOP) {
        SPDesktop *dt = SP_ACTIVE_DESKTOP;
        if (dt->shutdown()) {
            /* The user canceled the operation, so end doing the close */
            return FALSE;
        }
        // Shutdown can proceed; use the stored reference to the desktop here instead of the current SP_ACTIVE_DESKTOP,
        // because the user might have changed the focus in the meantime (see bug #381357 on Launchpad)
        dt->destroyWidget();
    }

    return TRUE;
}


void
sp_ui_dialog_title_string(Inkscape::Verb *verb, gchar *c)
{
    SPAction     *action;
    unsigned int shortcut;
    gchar        *s;
    gchar        *atitle;

    action = verb->get_action(Inkscape::ActionContext());
    if (!action)
        return;

    atitle = sp_action_get_title(action);

    s = g_stpcpy(c, atitle);

    g_free(atitle);

    shortcut = sp_shortcut_get_primary(verb);
    if (shortcut!=GDK_KEY_VoidSymbol) {
        gchar* key = sp_shortcut_get_label(shortcut);
        s = g_stpcpy(s, " (");
        s = g_stpcpy(s, key);
        g_stpcpy(s, ")");
        g_free(key);
    }
}


Glib::ustring getLayoutPrefPath( Inkscape::UI::View::View *view )
{
    Glib::ustring prefPath;

    if (reinterpret_cast<SPDesktop*>(view)->is_focusMode()) {
        prefPath = "/focus/";
    } else if (reinterpret_cast<SPDesktop*>(view)->is_fullscreen()) {
        prefPath = "/fullscreen/";
    } else {
        prefPath = "/window/";
    }

    return prefPath;
}


void
sp_ui_import_files(gchar *buffer)
{
    gchar** l = g_uri_list_extract_uris(buffer);
    for (unsigned int i=0; i < g_strv_length(l); i++) {
        gchar *f = g_filename_from_uri (l[i], nullptr, nullptr);
        sp_ui_import_one_file_with_check(f, nullptr);
        g_free(f);
    }
    g_strfreev(l);
}

static void
sp_ui_import_one_file_with_check(gpointer filename, gpointer /*unused*/)
{
    if (filename) {
        if (strlen((char const *)filename) > 2)
            sp_ui_import_one_file((char const *)filename);
    }
}

static void
sp_ui_import_one_file(char const *filename)
{
    SPDocument *doc = SP_ACTIVE_DOCUMENT;
    if (!doc) return;

    if (filename == nullptr) return;

    // Pass off to common implementation
    // TODO might need to get the proper type of Inkscape::Extension::Extension
    file_import( doc, filename, nullptr );
}

void
sp_ui_error_dialog(gchar const *message)
{
    GtkWidget *dlg;
    gchar *safeMsg = Inkscape::IO::sanitizeString(message);

    dlg = gtk_message_dialog_new(nullptr, GTK_DIALOG_DESTROY_WITH_PARENT, GTK_MESSAGE_ERROR,
                                 GTK_BUTTONS_CLOSE, "%s", safeMsg);
    sp_transientize(dlg);
    gtk_window_set_resizable(GTK_WINDOW(dlg), FALSE);
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
    g_free(safeMsg);
}

bool
sp_ui_overwrite_file(gchar const *filename)
{
    bool return_value = FALSE;

    if (Inkscape::IO::file_test(filename, G_FILE_TEST_EXISTS)) {
        Gtk::Window *window = SP_ACTIVE_DESKTOP->getToplevel();
        gchar* baseName = g_path_get_basename( filename );
        gchar* dirName = g_path_get_dirname( filename );
        GtkWidget* dialog = gtk_message_dialog_new_with_markup( window->gobj(),
                                                                (GtkDialogFlags)(GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT),
                                                                GTK_MESSAGE_QUESTION,
                                                                GTK_BUTTONS_NONE,
                                                                _( "<span weight=\"bold\" size=\"larger\">A file named \"%s\" already exists. Do you want to replace it?</span>\n\n"
                                                                   "The file already exists in \"%s\". Replacing it will overwrite its contents." ),
                                                                baseName,
                                                                dirName
            );
        gtk_dialog_add_buttons( GTK_DIALOG(dialog),
                                _("_Cancel"), GTK_RESPONSE_NO,
                                _("Replace"), GTK_RESPONSE_YES,
                                NULL );
        gtk_dialog_set_default_response( GTK_DIALOG(dialog), GTK_RESPONSE_YES );

        if ( gtk_dialog_run( GTK_DIALOG(dialog) ) == GTK_RESPONSE_YES ) {
            return_value = TRUE;
        } else {
            return_value = FALSE;
        }
        gtk_widget_destroy(dialog);
        g_free( baseName );
        g_free( dirName );
    } else {
        return_value = TRUE;
    }

    return return_value;
}

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
