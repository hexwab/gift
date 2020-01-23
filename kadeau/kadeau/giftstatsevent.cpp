/***************************************************************************
                          giftstatsevent.cpp  -  description
                             -------------------
    begin                : Tue Apr 2 2002
    copyright            : (C) 2002 by Robert Wittams
    email                : robert@wittams.com
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "giftstatsevent.h"
#include "xmltag.h"
#include <iostream>

giFTStatsEvent::giFTStatsEvent(unsigned long id){
  _id = id;
}

giFTStatsEvent::~giFTStatsEvent(){

}

void giFTStatsEvent::handleXml(XmlTag* tag){
      // Should be new protocol info.

      if ( tag->name().compare("stats") != 0){
          cerr<<"not stats tag"<<tag->toString() <<endl;
          return;
      }

      if( tag->value("protocol").isNull() ){
          // end tag.
          _isFinished = TRUE;
          emit finished(_id);
          return;
      }

       ProtocolInfo* prot= new ProtocolInfo;
       prot->protocol = tag->value("protocol");
       prot->status = tag->value("status");
       prot->users = tag->value("users").toULong();
       prot->files = tag->value("files").toULong();
       prot->size = tag->value("size").toULong();
       delete tag;
       emit newProtocol(prot);
}