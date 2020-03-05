// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Authors:
 *   Marc Jeanmougin <marc.jeanmougin@telecom-paris.fr>
 *
 * Copyright (C) 2019 Authors
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef __TRUC_H

#include "extension/implementation/implementation.h"


#include <glib.h>
#include <gmodule.h>
#include "inkscape-version.cpp"
#include "undo-stack-observer.h"
#include "io/stream/inkscapestream.h"
#include "xml/event.h"



namespace Inkscape {
namespace Extension {

class Effect;
class Extension;

namespace Internal {

class TrucObserver : public UndoStackObserver {
void notifyUndoCommitEvent(Event* log) override;
void notifyUndoEvent(Event* log) override;
void notifyRedoEvent(Event* log) override;
void notifyClearUndoEvent() override;
void notifyClearRedoEvent() override;
  public:

IO::StdWriter *writer;
};



class Truc : public Inkscape::Extension::Implementation::Implementation {


public:
    bool load(Inkscape::Extension::Extension *module) override;
    void effect(Inkscape::Extension::Effect *module, Inkscape::UI::View::View *document, Inkscape::Extension::Implementation::ImplementationDocumentCache * docCache) override;

private:
    TrucObserver *obs;
    bool enabled;
};

}; /* namespace Internal */
}; /* namespace Extension */
}; /* namespace Inkscape */

extern "C" G_MODULE_EXPORT Inkscape::Extension::Implementation::Implementation* GetImplementation() { return new Inkscape::Extension::Internal::Truc(); }
extern "C" G_MODULE_EXPORT const gchar* GetInkscapeVersion() { return Inkscape::version_string; }
#endif

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
