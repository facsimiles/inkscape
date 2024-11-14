// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2019, 2024 Marc Jeanmougin <marc.jeanmougin@telecom-paris.fr>
 * Copyright (C) 2020 Emmanuel Gil Peyrot <linkmauve@linkmauve.fr>
 *
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#ifndef __TRUC_H

#include "extension/implementation/implementation.h"


#include <gloox/client.h>
#include <gloox/loghandler.h>
#include <gloox/connectionlistener.h>
#include <gloox/jinglesessionhandler.h>
#include <gloox/jinglesessionmanager.h>
#include <gloox/jinglesession.h>
#include <glib.h>
#include <gmodule.h>
#include <memory>
#include "inkscape-version.cpp"
#include "undo-stack-observer.h"
#include "io/stream/inkscapestream.h"
#include "xml/event.h"

namespace gloox {
class Client;
}

namespace Inkscape {
namespace Extension {

class Effect;
class Extension;

namespace Internal {

class InkscapeClient : gloox::ConnectionListener, gloox::LogHandler, gloox::Jingle::SessionHandler {
public:
    InkscapeClient(gloox::JID jid, const std::string& password);
    bool connect();
    void disconnect();
    bool isConnected();
    gloox::ConnectionError recv();
    void send(gloox::Tag *tag);
    void sendChanges(gloox::JID recipient, std::string& sid, std::vector<gloox::Sxe::StateChange> state_changes);
    static int runLoop(void *data);
    gloox::JID jid() {return client->jid();}

private:
    // From ConnectionListener
    void onConnect() override;
    void onDisconnect(gloox::ConnectionError e) override;
    bool onTLSConnect(const gloox::CertInfo& info) override;

    // From LogHandler
    void handleLog(gloox::LogLevel level, gloox::LogArea area, const std::string& message) override;

    // From Jingle::SessionHandler
    void handleSessionAction(gloox::Jingle::Action action, gloox::Jingle::Session* session, const gloox::Jingle::Session::Jingle* jingle) override;
    void handleSessionActionError(gloox::Jingle::Action action, gloox::Jingle::Session* session, const gloox::Error* error) override;
    void handleIncomingSession(gloox::Jingle::Session* session) override;

    // std::vector<gloox::Sxe::StateChange> getCurrentState(const std::string& session, const std::string& id);

private:
    std::unique_ptr<gloox::Client> client;
    std::unique_ptr<gloox::Jingle::SessionManager> session_manager;
    bool connected;

// XXX: hack
public:
    std::string m_rid;
    std::string document_jid;
};


class XMPPObserver : public UndoStackObserver {
    void notifyUndoCommitEvent(Event* log) override;
    void notifyUndoEvent(Event* log) override;
    void notifyRedoEvent(Event* log) override;
    void notifyClearUndoEvent() override;
    void notifyClearRedoEvent() override;

public:
    XMPPObserver(std::shared_ptr<InkscapeClient> client);

    std::unique_ptr<IO::StdWriter> writer;
    std::shared_ptr<InkscapeClient> client;
};


class XMPP : public Inkscape::Extension::Implementation::Implementation {


public:
    bool load(Inkscape::Extension::Extension *module) override;
    void effect(Inkscape::Extension::Effect *module, SPDesktop *desktop, Inkscape::Extension::Implementation::ImplementationDocumentCache * docCache) override;

private:
    std::unique_ptr<XMPPObserver> obs;
    std::shared_ptr<InkscapeClient> client;
    unsigned int _event_source;
    bool enabled;
};

}; /* namespace Internal */
}; /* namespace Extension */
}; /* namespace Inkscape */

extern "C" G_MODULE_EXPORT Inkscape::Extension::Implementation::Implementation* GetImplementation() { return new Inkscape::Extension::Internal::XMPP(); }
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
