// SPDX-License-Identifier: GPL-3.0
/*
  Copyright (c) 2019-2023 by Jakob Schröter <js@camaya.net>
  This file is part of the gloox library. http://camaya.net/gloox

  This software is distributed under a license. The full license
  agreement can be found in the file LICENSE in this distribution.
  This software may not be copied, modified, sold or distributed
  other than expressed in the named license agreement.

  This software is distributed without any warranty.
*/

#include "sxe.h"
#include <gloox/tag.h>

#include <memory>
#include <string>

namespace gloox
{

  Sxe::Sxe( const Tag* tag )
    : StanzaExtension( ExtUser+1 )
  {
    if( !tag || tag->name() != "sxe" || tag->xmlns() != XMLNS_SXE )
      return;

    m_session = tag->findAttribute( "session" );
    m_id = tag->findAttribute( "id" );
    const TagList& l = tag->children();
    for( TagList::const_iterator it = l.begin(); it != l.end(); ++it )
    {
      const Tag* child = *it;
      if( child->xmlns() != XMLNS_SXE )
        break;

      if( child->name() == "connect" )
        m_type = SxeConnect;
      else if( child->name() == "state-offer" )
      {
        const TagList& l = child->children();
        for( TagList::const_iterator it = l.begin(); it != l.end(); ++it )
        {
          const Tag* description = *it;
          if( description->name() != "description" )
            return;
          m_state_offer_xmlns.push_back( description->xmlns() );
        }
        m_type = SxeStateOffer;
      }
      else if( child->name() == "accept-state" )
        m_type = SxeAcceptState;
      else if( child->name() == "refuse-state" )
        m_type = SxeRefuseState;
      else if( child->name() == "state" )
      {
        const TagList& l = child->children();
        for( TagList::const_iterator it = l.begin(); it != l.end(); ++it )
        {
          const Tag* child2 = *it;
          if( child2->xmlns() != XMLNS_SXE )
            return;

          StateChange change;
          if( child2->name() == "document-begin" )
          {
            change.type = StateChangeDocumentBegin;
            change.document_begin.prolog = child2->findAttribute( "prolog" ).c_str();
          }
          else if( child2->name() == "document-end" )
          {
            change.type = StateChangeDocumentEnd;
            change.document_end.last_sender = child2->findAttribute( "last-sender" ).c_str();
            change.document_end.last_id = child2->findAttribute( "last-id" ).c_str();
          }
          else if( child2->name() == "new" )
          {
            change.type = StateChangeNew;
            change.new_.rid = child2->findAttribute( "rid" ).c_str();
            change.new_.type = child2->findAttribute( "type" ).c_str();
            change.new_.name = child2->findAttribute( "name" ).c_str();
            change.new_.ns = child2->findAttribute( "ns" ).c_str();
            change.new_.chdata = child2->findAttribute( "chdata" ).c_str();
          }
          else if( child2->name() == "remove" )
          {
            change.type = StateChangeRemove;
            change.remove.target = child2->findAttribute( "target" ).c_str();
          }
          else if( child2->name() == "set" )
          {
            change.type = StateChangeSet;
            change.set.target = child2->findAttribute( "target" ).c_str();
            change.set.version = child2->findAttribute( "version" ).c_str();
            change.set.name = child2->findAttribute( "name" ).c_str();
            change.set.ns = child2->findAttribute( "ns" ).c_str();
            change.set.chdata = child2->findAttribute( "chdata" ).c_str();
          }
          else
          {
            return;
          }
          m_state_changes.push_back( change );
        }
        m_type = SxeState;
      }
    }
  }

  Tag* Sxe::tag() const
  {
    if ( m_type == SxeInvalid )
      return 0;

    Tag* t = new Tag( "sxe" );
    t->setXmlns( XMLNS_SXE );
    t->addAttribute( "session", m_session );
    t->addAttribute( "id", m_id );

    if( m_type == SxeConnect )
      new Tag( t, "connect" );
    else if( m_type == SxeAcceptState )
      new Tag( t, "accept-state" );
    else if( m_type == SxeRefuseState )
      new Tag( t, "refuse-state" );
    else if( m_type == SxeStateOffer )
    {
      Tag* child = new Tag( t, "state-offer" );
      child->setXmlns( XMLNS_SXE );
      for( std::vector<std::string>::const_iterator it = m_state_offer_xmlns.begin(); it != m_state_offer_xmlns.end(); ++it )
      {
        const std::string& xmlns = *it;
        Tag* description = new Tag( child, "description" );
        description->setXmlns( xmlns );
      }
    }
    else if( m_type == SxeState )
    {
      Tag* state = new Tag( t, "state" );
      state->setXmlns( XMLNS_SXE );
      for( std::vector<StateChange>::const_iterator it = m_state_changes.begin(); it != m_state_changes.end(); ++it )
      {
        const StateChange& change = *it;
        Tag* child;
        if( change.type == StateChangeDocumentBegin )
        {
          child = new Tag( state, "document-begin" );
          child->addAttribute( "prolog", change.document_begin.prolog );
        }
        else if( change.type == StateChangeDocumentEnd )
        {
          child = new Tag( state, "document-end" );
          child->addAttribute( "last-sender", change.document_end.last_sender );
          child->addAttribute( "last-id", change.document_end.last_id );
        }
        else if( change.type == StateChangeNew )
        {
          child = new Tag( state, "new" );
          child->addAttribute( "rid", change.new_.rid );
          child->addAttribute( "type", change.new_.type );
          child->addAttribute( "name", change.new_.name );
          child->addAttribute( "ns", change.new_.ns );
          child->addAttribute( "chdata", change.new_.chdata );
        }
        else if( change.type == StateChangeRemove )
        {
          child = new Tag( state, "remove" );
          child->addAttribute( "target", change.remove.target );
        }
        else if( change.type == StateChangeSet )
        {
          child = new Tag( state, "set" );
          child->addAttribute( "target", change.set.target );
          child->addAttribute( "version", change.set.version );
          child->addAttribute( "name", change.set.name );
          child->addAttribute( "ns", change.set.ns );
          child->addAttribute( "chdata", change.set.chdata );
        }
        else
        {
          return 0;
        }
      }
    }

    return t;
  }

  const std::string& Sxe::filterString() const
  {
    static const std::string filter = "/message/sxe[@xmlns='" + XMLNS_SXE + "']";
    return filter;
  }

}
