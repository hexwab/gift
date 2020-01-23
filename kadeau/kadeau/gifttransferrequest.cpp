/***************************************************************************
                          gifttransferrequest.cpp  -  description
                             -------------------
    begin                : Mon Apr 1 2002
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

#include "gifttransferrequest.h"
#include "gifttransferdownloadevent.h"
#include "xmltag.h"
#include "qstring.h"

#include <iostream>

giFTTransferRequest::giFTTransferRequest( int sz, QString hsh, QString fname, QList<giFTSearchItem>* _sources){
	size = sz;
	hash = hsh;
	filename = fname;
  sources = _sources;		
}

giFTTransferRequest::~giFTTransferRequest(){
}

giFTEvent* giFTTransferRequest::makeEvent(XmlTag *tag){
	  unsigned long id;

    // check it is a transfer event.
		if( tag->name().compare("event") != 0 )
					return NULL;
		
		if( tag->value("action") != "transfer")
          return NULL;

		id = tag->value("id").toULong();
		delete tag;
		
    giFTEvent* ev = new giFTTransferDownloadEvent(id, size, hash, filename, sources);
		emit madeEvent(ev);
		return ev;
}

XmlTag* giFTTransferRequest::xml(){
		XmlTag* tag = new XmlTag();
		tag->setName("transfer");
		
		tag->append("action", "download");
		tag->append("size", QString::number(size));
		tag->append("hash", hash);
		tag->append("save", filename);

		return tag;
}