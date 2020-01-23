/***************************************************************************
                          giftstatsrequest.cpp  -  description
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

#include "giftstatsrequest.h"
#include "giftstatsevent.h"
#include "xmltag.h"

giFTStatsRequest::giFTStatsRequest(){
}

giFTStatsRequest::~giFTStatsRequest(){
}

giFTEvent* giFTStatsRequest::makeEvent(XmlTag *tag){
    unsigned long  id;

    // check it is a stats event.
    if( tag->name().compare("event") != 0 )
        return NULL;

    if( tag->value("action") != "stats")
          return NULL;

    id = tag->value("id").toULong();
    delete tag;

    giFTStatsEvent* event = new giFTStatsEvent(id);
    emit madeEvent(event);
    return event;
}

XmlTag* giFTStatsRequest::xml(){
    XmlTag* tag = new XmlTag();
    tag->setName("stats");
    return tag;
}

