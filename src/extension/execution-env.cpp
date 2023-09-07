// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Ted Gould <ted@gould.cx>
 *   Abhishek Sharma
 *
 * Copyright (C) 2007-2008 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <gtkmm/dialog.h>
#include <gtkmm/messagedialog.h>

#include "execution-env.h"
#include "prefdialog/prefdialog.h"
#include "implementation/implementation.h"
#include "actions/actions-helper.h"
#include "inkscape-application.h"

#include "selection.h"
#include "effect.h"
#include "document.h"
#include "desktop.h"
#include "inkscape.h"
#include "document-undo.h"
#include "desktop.h"
#include "object/sp-namedview.h"

#include "ui/widget/canvas.h"  // To get window (perverse!)

namespace Inkscape {
namespace Extension {

/** \brief  Create an execution environment that will allow the effect
            to execute independently.
    \param effect  The effect that we should execute
    \param desktop   The desktop containing the document to execute on
    \param docCache  The cache created for that document
    \param show_working  Show the working dialog
    \param show_error    Show the error dialog (not working)

    Grabs the selection of the current document so that it can get
    restored.  Will generate a document cache if one isn't provided.
*/
ExecutionEnv::ExecutionEnv (Effect * effect, SPDesktop * desktop, Implementation::ImplementationDocumentCache * docCache, bool show_working, bool show_errors) :
    _state(ExecutionEnv::INIT),
    _visibleDialog(nullptr),
    _mainloop(nullptr),
    _desktop(desktop),
    _docCache(docCache),
    _effect(effect),
    _show_working(show_working)
{
    auto app = InkscapeApplication::instance();
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        std::cerr << "No selection or document" << std::endl;
    }
    // Temporarily prevent undo in this scope
    Inkscape::DocumentUndo::ScopedInsensitive pauseUndo(document);
    selection->enforceIds();
    genDocCache();

    return;
}

/** \brief  Destroy an execution environment

    Destroys the dialog if created and the document cache.
*/
ExecutionEnv::~ExecutionEnv () {
    if (_visibleDialog != nullptr) {
        _visibleDialog->set_visible(false);
        delete _visibleDialog;
        _visibleDialog = nullptr;
    }
    killDocCache();
    return;
}

/** \brief  Generate a document cache if needed

    If there isn't one we create a new one from the implementation
    from the effect's implementation.
*/
void
ExecutionEnv::genDocCache () {
    if (_docCache == nullptr) {
        // printf("Gen Doc Cache\n");
        _docCache = _effect->get_imp()->newDocCache(_effect, _desktop);
    }
    return;
}

/** \brief  Destroy a document cache

    Just delete it.
*/
void
ExecutionEnv::killDocCache () {
    if (_docCache != nullptr) {
        // printf("Killed Doc Cache\n");
        delete _docCache;
        _docCache = nullptr;
    }
    return;
}

/** \brief  Create the working dialog

    Builds the dialog with a message saying that the effect is working.
    And make sure to connect to the cancel.
*/
void
ExecutionEnv::createWorkingDialog () {
    if (!_desktop) {
        return;
    }
    if (_visibleDialog != nullptr) {
        _visibleDialog->set_visible(false);
        delete _visibleDialog;
        _visibleDialog = nullptr;
    }

    Gtk::Widget *toplevel = _desktop->getCanvas()->get_toplevel();
    Gtk::Window *window = dynamic_cast<Gtk::Window *>(toplevel);
    if (!window) {
        return;
    }

    gchar * dlgmessage = g_strdup_printf(_("'%s' complete, loading result..."), _effect->get_name());
    _visibleDialog = new Gtk::MessageDialog(*window,
                               dlgmessage,
                               false, // use markup
                               Gtk::MESSAGE_INFO,
                               Gtk::BUTTONS_CANCEL,
                               true); // modal
    _visibleDialog->signal_response().connect(sigc::mem_fun(*this, &ExecutionEnv::workingCanceled));
    g_free(dlgmessage);

    Gtk::Dialog *dlg = _effect->get_pref_dialog();
    if (dlg) {
        _visibleDialog->set_transient_for(*dlg);
    } else {
        // ToDo: Do we need to make the window transient for the main window here?
        //       Currently imossible to test because of GUI freezing during save,
        //       see https://bugs.launchpad.net/inkscape/+bug/967416
    }
    _visibleDialog->show_now();

    return;
}

void
ExecutionEnv::workingCanceled( const int /*resp*/) {
    cancel();
    undo();
    return;
}

void
ExecutionEnv::cancel () {
    if (_desktop) {
        _desktop->clearWaitingCursor();
    }
    _effect->get_imp()->cancelProcessing();
    return;
}

void
ExecutionEnv::undo () {
    auto app = InkscapeApplication::instance();
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        std::cerr << "No selection or document" << std::endl;
    }
    DocumentUndo::cancel(document);
    return;
}

void
ExecutionEnv::commit () {
    auto app = InkscapeApplication::instance();
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        std::cerr << "No selection or document" << std::endl;
    }
    DocumentUndo::done(document, _effect->get_name(), "");
    Effect::set_last_effect(_effect);
    _effect->get_imp()->commitDocument();
    killDocCache();
    return;
}

void
ExecutionEnv::reselect () {
    auto app = InkscapeApplication::instance();
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        return;
    }
    selection->restoreBackup();
}

void
ExecutionEnv::run (std::list<std::string> &params) {
    _state = ExecutionEnv::RUNNING;
    auto app = InkscapeApplication::instance();
    SPDocument* document = nullptr;
    Inkscape::Selection* selection = nullptr;
    if (!get_document_and_selection(app, &document, &selection)) {
        std::cerr << "No selection or document" << std::endl;
    }
    if (_desktop && _show_working) {
        createWorkingDialog();
    }
    selection->setBackup();
    if (_desktop) {
        _desktop->setWaitingCursor();
    }
    _effect->get_imp()->effect(_effect, _desktop, _docCache, params);
    Effect::set_last_params(params);
    if (_desktop) {
        _desktop->clearWaitingCursor();
    }
    _state = ExecutionEnv::COMPLETE;
    selection->restoreBackup();
    // _runComplete.signal();
    return;
}

void
ExecutionEnv::runComplete () {
    _mainloop->quit();
}

bool
ExecutionEnv::wait () {
    if (_state != ExecutionEnv::COMPLETE) {
        if (_mainloop) {
            _mainloop = Glib::MainLoop::create(false);
        }

        sigc::connection conn = _runComplete.connect(sigc::mem_fun(*this, &ExecutionEnv::runComplete));
        _mainloop->run();

        conn.disconnect();
    }

    return true;
}



} }  /* namespace Inkscape, Extension */



/*
  Local Variables:
  mode:c++
  c-file-style:"stroustrup"
  c-file-offsets:((innamespace . 0)(inline-open . 0)(case-label . +))
  indent-tabs-mode:nil
  fill-column:99
  End:
*/
// vim: filetype=cpp:expandtab:shiftwidth=4:tabstop=8:softtabstop=4 :
