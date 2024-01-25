// SPDX-License-Identifier: GPL-2.0-or-later
/**
    \file xmpp.cpp

    A collaborative edition plugin.
*/
/*
 * Copyright (C) 2019 Marc Jeanmougin <marc.jeanmougin@telecom-paris.fr>
 * Copyright (C) 2020 Emmanuel Gil Peyrot <linkmauve@linkmauve.fr>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include <gloox/tag.h>
#include <gloox/jid.h>
#include <gloox/client.h>
#include <gloox/disco.h>
#include <gloox/message.h>
#include <gloox/jinglesessionmanager.h>
#include <gloox/jinglecontent.h>
#undef lookup
#include "sxe.h"

#include <gtkmm/adjustment.h>
#include <gtkmm/box.h>
#include <gtkmm/spinbutton.h>

#include "desktop.h"

#include <2geom/geom.h>
#include "event.h"
#include "document.h"
#include "object/sp-object.h"
#include "selection.h"
#include "xml/attribute-record.h"
#include "inkscape-version.h"

#include "svg/path-string.h"

#include "extension/effect.h"
#include "extension/system.h"

#include "util/units.h"

#include "xmpp.h"

namespace Inkscape {
namespace Extension {
namespace Internal {

using namespace gloox;

std::string get_uuid()
{
    // TODO: use a real UUID.
    char attr_rid[11];
    snprintf(attr_rid, 11, "%d", rand());
    return std::string(attr_rid);
}


// TODO: this is a hack, this namespace isn’t reserved so shouldn’t be used, we
// probably want to change the XEP to use an application based on the MIME type
// of the document or something like that.
class SvgApplication : public Jingle::Plugin
{
public:
    SvgApplication(const Tag* tag = 0)
        : Jingle::Plugin(Jingle::PluginUser)
    {}

    // reimplemented from Plugin
    const StringList features() const override
    {
        StringList sl;
        sl.push_back("urn:xmpp:jingle:apps:svg");
        return sl;
    }

    // reimplemented from Plugin
    const std::string& filterString() const override
    {
        static const std::string filter = "content[@xmlns='" + XMLNS_JINGLE + "']/transport[@xmlns='urn:xmpp:jingle:apps:svg']";
        return filter;
    }

    // reimplemented from Plugin
    Tag* tag() const override
    {
        return new Tag("description", XMLNS, "urn:xmpp:jingle:apps:svg");
    }

    // reimplemented from Plugin
    Jingle::Plugin* newInstance(const Tag* tag) const override
    {
        return new SvgApplication(tag);
    }

    // reimplemented from Plugin
    Jingle::Plugin* clone() const override
    {
	return new SvgApplication(*this);
    }
};

InkscapeClient::InkscapeClient(JID jid, const std::string& password)
{
    client = std::make_unique<Client>(jid, password);
    // TODO: figure out why SCRAM-SHA-1 isn’t working.
    client->setSASLMechanisms(SaslMechPlain);
    // TODO: fetch the OS properly, instead of hardcoding it to Linux.
    client->disco()->setVersion("Inkscape", version_string_without_revision, "Linux");
    client->disco()->setIdentity("collaboration", "whiteboard", "Inkscape");
    session_manager = std::unique_ptr<Jingle::SessionManager>(new Jingle::SessionManager(client.get(), this));
    session_manager->registerPlugin(new Jingle::Content());
    client->registerConnectionListener(this);
    //client->logInstance().registerLogHandler(LogLevelDebug, LogAreaXmlOutgoing | LogAreaXmlIncoming, this);
    client->logInstance().registerLogHandler(LogLevelDebug, ~0, this);
}

bool InkscapeClient::connect()
{
    return client->connect(false);
}

void InkscapeClient::disconnect()
{
    client->disconnect();
    connected = false;
}

bool InkscapeClient::isConnected()
{
    return connected;
}

ConnectionError InkscapeClient::recv()
{
    // Return immediately if no data was available on the socket.
    return client->recv(0);
}

void InkscapeClient::send(Tag *tag)
{
    client->send(tag);
}

void InkscapeClient::sendChanges(JID recipient, std::string& sid, std::vector<Sxe::StateChange> state_changes)
{
    //TODO
    // client->send(something)
    //TODO sxe_manager->sendChanges(recipient, sid, state_changes);
}

int InkscapeClient::runLoop(void *data)
{
    InkscapeClient *client = static_cast<InkscapeClient*>(data);
    ConnectionError err = client->recv();
    if (err != ConnNoError) {
        printf("Error while receiving on gloox socket: %d\n", err);
        return FALSE;
    }

    return TRUE;
}

// From ConnectionListener
void InkscapeClient::onConnect()
{
    printf("connected!\n");
    connected = true;
}

void InkscapeClient::onDisconnect(ConnectionError e)
{
    printf("disconnected\n");
    connected = false;
}

bool InkscapeClient::onTLSConnect(const CertInfo& info)
{
    printf("accept cert? yes of course\n");
    return true;
}

// From LogHandler
void InkscapeClient::handleLog(LogLevel level, LogArea area, const std::string& message)
{
    switch (area) {
    case LogAreaXmlIncoming:
        printf("RECV %s\n", message.c_str());
        break;
    case LogAreaXmlOutgoing:
        printf("SEND %s\n", message.c_str());
        break;
    default:
        printf("gloox: %s\n", message.c_str());
        break;
    }
    fflush(stdout);
}

void InkscapeClient::handleSessionAction(Jingle::Action action, Jingle::Session* session, const Jingle::Session::Jingle* jingle)
{
    printf("handleSessionAction(action=%d, session=%p, jingle=%p)\n", action, session, jingle);

    switch (action) {
    case Jingle::SessionInitiate: {
        std::string name;
        printf("plugins: %zd\n", jingle->plugins().size());
        for (const Jingle::Plugin* p : jingle->plugins()) {
            printf("- %p\n", p);
            // XXX: Don’t assume this is a Jingle::Content…
            const Jingle::Content* content = reinterpret_cast<const Jingle::Content*>(p);
            name = content->name();
            break;
        }
        printf("that’s it!\n");
        fflush(stdout);
        if (true/*accept*/) {
            std::list<const Jingle::Plugin*> plugins_list;
            SvgApplication* description = new SvgApplication();
            plugins_list.push_front(description);
            bool ret = session->sessionAccept(new Jingle::Content(name, plugins_list));
            printf("accepted? %d\n", ret);
        } else {
            bool ret = session->sessionTerminate(new Jingle::Session::Reason(Jingle::Session::Reason::UnsupportedApplications));
            printf("terminated? %d\n", ret);
        }
        fflush(stdout);
        break;
    }
    default:
        printf("Unhandled…\n");
        break;
    }
}

void InkscapeClient::handleSessionActionError(Jingle::Action action, Jingle::Session* session, const Error* error )
{
    printf("handleSessionActionError(action=%d, session=%p, error=%p)\n", action, session, error);
}

void InkscapeClient::handleIncomingSession(Jingle::Session* session)
{
    printf("handleIncomingSession(session=%p)\n", session);
}
/*
std::vector<StateChangeType> InkscapeClient::getCurrentState(const std::string& session, const std::string& id)
{
    printf("getCurrentState(session=%s, id=%s)\n", session.c_str(), id.c_str());
    std::vector<StateChangeType> state;
    Sxe::DocumentBegin document_begin = {
        .prolog = "data:image/svg+xml,<?xml version=\"1.0\" encoding=\"UTF-8\"?>",
    };
    StateChangeType begin = {
        .type = StateChangeTypeDocumentBegin,
        .document_begin = document_begin,
    };
    state.push_back(begin);
    m_rid = get_uuid();
    gloox::New new_ = gloox::New::Element(
        /*rid* /  m_rid.c_str(),
        /*ns* /   "http://www.w3.org/2000/svg",
        /*name* / "svg"
    );
    StateChangeType change = {
        .type = StateChangeTypeNew,
        .new_ = new_,
    };
    state.push_back(change);
    Sxe::DocumentEnd document_end = {
        .last_sender = "foo@bar/baz",
        .last_id = "unknown",
    };
    StateChangeType end = {
        .type = StateChangeTypeDocumentEnd,
        .document_end = document_end,
    };
    state.push_back(end);
    return state;
}*/

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
            XML::Node *node = eadd->child;

            std::string rid = get_uuid();
            std::string name = node->name();
            if (name.substr(0, 4) != "svg:") {
                printf("Wrong prefix \"%s\"!\n", name.substr(0, 4).c_str());
                abort();
            }
            name = name.substr(4);

            Sxe::New new_ = {
                /*rid*/  rid.c_str(),
                /*parent*/  client->m_rid.c_str(),
                /*ns*/   "http://www.w3.org/2000/svg",
                /*name*/ name.c_str(),
                "",
                ""
            };
            Sxe::StateChange change = {
                .type = Sxe::StateChangeNew,
                .new_ = new_
            };
            std::vector<Sxe::StateChange> state_changes = {};
            state_changes.push_back(change);

            // XXX: huge hack to keep all rids on the stack…
            size_t num_attrs = 0;
            for (auto it : node->attributeList()) {
                ++num_attrs;
            }
            std::vector<std::string> rids;
            rids.reserve(num_attrs);

            size_t cur_attr = 0;
            for (auto it : node->attributeList()) {
                rids.push_back(get_uuid());

                Sxe::New new_ = {
                    /*rid*/    rids[cur_attr].c_str(),
                    /*parent*/ rid.c_str(),
                    /*name*/   g_quark_to_string(it.key),
                    /*chdata*/ it.value,
                    "",
                    ""
                };
                Sxe::StateChange change = {
                    .type = Sxe::StateChangeNew,
                    .new_ = new_,
                };
                state_changes.push_back(change);
                ++cur_attr;
            }

            std::vector<Sxe::StateChange> copy(state_changes);
            Sxe* coucou = new Sxe("session", "id", Sxe::SxeStateOffer, {XMLNS_SXE}, copy);
            fprintf(stderr, "coucou: %s\n", coucou->tag()->xml().c_str());

            //Message msg(Message::Normal, JID("linkmauve@linkmauve.fr"));
            //msg.addExtension(new Sxe("session", "id", Sxe::TypeState, {}, state_changes));
            std::string sid = "foo";
            client->sendChanges(JID("test@xmpp.r2.enst.fr/test2"), sid, state_changes);
            printf("coucou\n");
        } else if ((edel = dynamic_cast<XML::EventDel *>(e))) {
            std::cout << "EventDel" << std::endl;
            sp_repr_write_stream(edel->child, *writer, 0, false, GQuark(0), 0, 0);
            printf("\n");

            Sxe::StateChange change = {
                .type = Sxe::StateChangeRemove,
                .remove = Sxe::Remove {
                    .target = "coucou",
                },
            };
            std::vector<Sxe::StateChange> state_changes = {};
            state_changes.push_back(change);

            Message msg(Message::Normal, JID("test@xmpp.r2.enst.fr"));
            msg.addExtension(new Sxe("session", "id", Sxe::SxeState, {}, state_changes));
            client->send(msg.tag());
        } else if ((echga = dynamic_cast<XML::EventChgAttr *>(e))) {
            std::cout << "EventChgAttr" << std::endl;
            printf("%s from %s to %s\n", g_quark_to_string(echga->key), &*(echga->oldval), &*(echga->newval));
        } else if ((echgc = dynamic_cast<XML::EventChgContent *>(e))) {
            std::cout << "EventChgContent" << std::endl;
            printf("%s to %s\n", &*(echgc->oldval), &*(echgc->newval));
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

XMPPObserver::XMPPObserver(std::shared_ptr<InkscapeClient> client)
    : client(client)
{}

/**
    \brief  A function to allocated anything -- just an example here
    \param  module  Unused
    \return Whether the load was successful
*/
bool XMPP::load(Inkscape::Extension::Extension * /*module*/)
{
    enabled = false;

    // TODO: fetch these from the preferences.
    JID jid("test@xmpp.r2.enst.fr");
    const char *password = "test";

    client = std::make_shared<InkscapeClient>(jid, password);
    client->connect();

    // TODO: find a better way to integrate gloox’s fd into the main loop.
    g_timeout_add(16, &InkscapeClient::runLoop, client.get());

    obs = std::unique_ptr<XMPPObserver>(new XMPPObserver(client));
    obs->writer = std::unique_ptr<IO::StdWriter>(new IO::StdWriter());
    return TRUE;
}

/**
    \brief  This actually draws the grid.
    \param  module   The effect that was called (unused)
    \param  document What should be edited.
*/
void XMPP::effect(Inkscape::Extension::Effect *module, SPDesktop *desktop,
                  Inkscape::Extension::Implementation::ImplementationDocumentCache * /*docCache*/)
{
    std::cout << (enabled ? "disabling" : "enabling") << std::endl;
    if (!enabled)
        desktop->doc()->addUndoObserver(*obs);
    else
        desktop->doc()->removeUndoObserver(*obs);
    enabled = !enabled;
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
