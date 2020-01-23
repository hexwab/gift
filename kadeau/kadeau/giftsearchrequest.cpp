/***************************************************************************
                          giftsearchrequest.cpp  -  description
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

#include "giftsearchrequest.h"
#include "giftsearchevent.h"
#include "xmltag.h"
#include <iostream>

giFTSearchRequest::giFTSearchRequest(QString q ,
										QString e ="",
										QString p ="",
										QString t ="",
										QString r ="",
										QString cs ="",
										QString cb ="" ){
	query = q; exclude = e ; protocol = p; type = t;
	realm = r; csize = cs; ckbps = cb; 	
}


giFTSearchRequest::~giFTSearchRequest(){

}

giFTEvent* giFTSearchRequest::makeEvent(XmlTag *tag){
	  unsigned long  id;
	
		
		
    // check it is a search event.
		if( tag->name().compare("event") != 0 )
					return NULL;
		
		if( tag->value("action") != "search")
          return NULL;

		id = tag->value("id").toULong();
		delete tag;

		giFTEvent* ev = new giFTSearchEvent(id);
		emit madeEvent(ev);
		return ev;
}


XmlTag* giFTSearchRequest::xml(){
		XmlTag* tag = new XmlTag();
		tag->setName("search");
		
		tag->append("query", query);

		tag->append("exclude", exclude);		
		tag->append("protocol", protocol);
		tag->append("type", type);
		tag->append("realm", realm);
		tag->append("csize", csize);
		tag->append("ckbps", ckbps);

		return tag;
}

