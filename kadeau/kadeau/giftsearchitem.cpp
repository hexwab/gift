/***************************************************************************
                          giftsearchitem.cpp  -  description
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

#include "giftsearchitem.h"
#include "xmltag.h"


giFTSearchItem::giFTSearchItem(XmlTag* tag){
  _user = tag->value("user");
  _node = tag->value("node");
  _href = tag->value("href");
  _size = tag->value("size").toULong();
  _hash = tag->value("hash");
  _availability = tag->value("availability");
  _key.append(_hash);
  _key.append(QString::number(_size));
  _hits = 0;
}


giFTSearchItem::~giFTSearchItem(){
}


void giFTSearchItem::addHit(){
    _hits++;
    emit hitAdded(_hits);
}