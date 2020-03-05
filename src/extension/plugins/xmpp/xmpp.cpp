// SPDX-License-Identifier: GPL-2.0-or-later
/**
    \file truc.cpp

    A test plug-in.
*/
/*
 * Copyright (C) 2019 Marc Jeanmougin <marc.jeanmougin@telecom-paris.fr>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <gtkmm/adjustment.h>
#include <gtkmm/box.h>
#include <gtkmm/spinbutton.h>

#include "desktop.h"

#include "2geom/geom.h"
#include "document.h"
#include "object/sp-object.h"
#include "selection.h"

#include "svg/path-string.h"

#include "extension/effect.h"
#include "extension/system.h"

#include "util/units.h"

#include "xmpp.h"

namespace Inkscape {
namespace Extension {
namespace Internal {

void XMPPObserver::notifyUndoCommitEvent(Event *ee)
{
    XML::Event *e = ee->event;
    std::cout << "UndoCommitEvent" << std::endl;

    while (e) {
        // printf("DOCUMENT %p\n", e->repr->document());
        if (e->repr)
            printf("AFFECTED %s\n", e->repr->attribute("id"));
        XML::EventAdd *eadd;
        XML::EventDel *edel;
        XML::EventChgAttr *echga;
        XML::EventChgContent *echgc;
        XML::EventChgOrder *echgo;
        XML::EventChgElementName *echgn;

        if ((eadd = dynamic_cast<XML::EventAdd *>(e))) {
            std::cout << "EventAdd" << std::endl;
            sp_repr_write_stream(eadd->child, *writer, 0, false, GQuark(0), 0, 0);
            printf("\n");
        } else if ((edel = dynamic_cast<XML::EventDel *>(e))) {
            std::cout << "EventDel" << std::endl;
            sp_repr_write_stream(edel->child, *writer, 0, false, GQuark(0), 0, 0);
            printf("\n");
        } else if ((echga = dynamic_cast<XML::EventChgAttr *>(e))) {
            std::cout << "EventChgAttr" << std::endl;
            printf("%s to %s", &*(echga->oldval), &*(echga->newval));
            printf("\n");
        } else if ((echgc = dynamic_cast<XML::EventChgContent *>(e))) {
            std::cout << "EventChgContent" << std::endl;
            printf("%s to %s", &*(echgc->oldval), &*(echgc->newval));
            printf("\n");
        } else if ((echgo = dynamic_cast<XML::EventChgOrder *>(e))) {
            std::cout << "EventChgOrder" << std::endl;
        } else if ((echgn = dynamic_cast<XML::EventChgElementName *>(e))) {
            std::cout << "EventChgElementName" << std::endl;
        } else {
            std::cout << "Unknown event" << std::endl;
        }

        e = e->next;
    }
}
void XMPPObserver::notifyUndoEvent(Event *e)
{
    std::cout << "UndoEvent" << std::endl;
    this->notifyUndoCommitEvent(e);
}
void XMPPObserver::notifyRedoEvent(Event *e)
{
    std::cout << "RedoEvent" << std::endl;
    this->notifyUndoCommitEvent(e);
}
void XMPPObserver::notifyClearUndoEvent() { std::cout << "ClearUndoEvent" << std::endl; }
void XMPPObserver::notifyClearRedoEvent() { std::cout << "ClearRedoEvent" << std::endl; }

/**
    \brief  A function to allocated anything -- just an example here
    \param  module  Unused
    \return Whether the load was successful
*/
bool XMPP::load(Inkscape::Extension::Extension * /*module*/)
{
    this->obs = new XMPPObserver();
    this->obs->writer = new IO::StdWriter();
    this->enabled = false;
    std::cout << "Hey, I'm TRUE, I'm loading!" << std::endl;
    return TRUE;
}

/**
    \brief  This actually draws the grid.
    \param  module   The effect that was called (unused)
    \param  document What should be edited.
*/
void XMPP::effect(Inkscape::Extension::Effect *module, Inkscape::UI::View::View *document,
                  Inkscape::Extension::Implementation::ImplementationDocumentCache * /*docCache*/)
{
    std::cout << (enabled ? "disabling" : "enabling") << std::endl;
    if (!this->enabled)
        document->doc()->addUndoObserver(*obs);
    else
        document->doc()->removeUndoObserver(*obs);
    this->enabled = !this->enabled;
}



}; /* namespace Internal */
}; /* namespace Extension */
}; /* namespace Inkscape */


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
