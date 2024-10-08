// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 * David Yip <yipdw@alumni.rose-hulman.edu>
 *   Abhishek Sharma
 *
 * Copyright (c) 2006 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef SEEN_INKSCAPE_CONSOLE_OUTPUT_UNDO_OBSERVER_H
#define SEEN_INKSCAPE_CONSOLE_OUTPUT_UNDO_OBSERVER_H

#include "undo-stack-observer.h"

namespace Inkscape {

/**
 * Inkscape::ConsoleOutputUndoObserver - observer for tracing calls to
 * SPDocumentUndo::undo, SPDocumentUndo::redo, SPDocumentUndo::maybe_done.
 *
 */
class ConsoleOutputUndoObserver : public UndoStackObserver {
public:
    ConsoleOutputUndoObserver() : UndoStackObserver() { }
    ~ConsoleOutputUndoObserver() override = default;

    void notifyUndoEvent(Event* log) override;
    void notifyRedoEvent(Event* log) override;
    void notifyUndoCommitEvent(Event* log) override;
    void notifyUndoExpired(Event *log) override;
    void notifyClearUndoEvent() override;
    void notifyClearRedoEvent() override;

};
}

#endif // SEEN_INKSCAPE_CONSOLE_OUTPUT_UNDO_OBSERVER_H

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
