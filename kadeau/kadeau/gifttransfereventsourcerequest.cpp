/***************************************************************************
                          gifttransfereventsourcerequest.cpp  -  description
                             -------------------
    begin                : Wed Apr 3 2002
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

#include "gifttransfereventsourcerequest.h"
#include "giftsearchitem.h"
#include "xmltag.h"
#include <iostream>

giFTTransferEventSourceRequest::giFTTransferEventSourceRequest(unsigned int _id, giFTSearchItem* item, bool _add = TRUE){
  id = _id;
  user = item->user();
  href = item->href();
  add =  _add;
}



giFTTransferEventSourceRequest::~giFTTransferEventSourceRequest(){

}

XmlTag* giFTTransferEventSourceRequest::xml(){
	  XmlTag* tag = new XmlTag();

		tag->setName("transfer");
		tag->append("id", QString::number(id));
    tag->append("user", user);
		QString attrib =  add ? "addsource" : "delsource";
		tag->append(attrib, href);
	 	return tag;
}