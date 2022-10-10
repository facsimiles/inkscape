// SPDX-License-Identifier: GPL-2.0-or-later
/** @file
 * Object building an event log.
 *//*
 * Authors: see git history
 *
 * Copyright (C) 2018 Authors
 * Copyright 2005 MenTaLguY <mental@rydia.net>
 * 
 * Released under GNU GPL v2+, read the file 'COPYING' for more information.
 */

#include "xml/log-builder.h"
#include "xml/event.h"
#include "xml/event-fns.h"

namespace Inkscape {
namespace XML {

void LogBuilder::discard() {
    sp_repr_free_log(_log);
    _log = nullptr;
}

Event *LogBuilder::detach() {
    Event *log=_log;
    _log = nullptr;
    return log;
}

void LogBuilder::addChild(Node &node, Node &child, Node *prev) {
    _log = new Inkscape::XML::EventAdd(&node, &child, prev, _log);
    _log = _log->optimizeOne();
}

void LogBuilder::removeChild(Node &node, Node &child, Node *prev) {
    _log = new Inkscape::XML::EventDel(&node, &child, prev, _log);
    _log = _log->optimizeOne();
}

void LogBuilder::setChildOrder(Node &node, Node &child,
                               Node *old_prev, Node *new_prev)
{
    _log = new Inkscape::XML::EventChgOrder(&node, &child, old_prev, new_prev, _log);
    _log = _log->optimizeOne();
}

void LogBuilder::setContent(Node &node,
                            char const *old_content,
                            char const *new_content)
{
    _log = new Inkscape::XML::EventChgContent(&node, old_content, new_content, _log);
    _log = _log->optimizeOne();
}

void LogBuilder::setAttribute(Node &node, GQuark name,
                              char const *old_value,
                              char const *new_value)
{
    _log = new Inkscape::XML::EventChgAttr(&node, name, old_value, new_value, _log);
    _log = _log->optimizeOne();
}

void LogBuilder::setElementName(Node& node, GQuark old_name, GQuark new_name)
{
    _log = new Inkscape::XML::EventChgElementName(&node, old_name, new_name, _log);
    _log = _log->optimizeOne();
}

}
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
