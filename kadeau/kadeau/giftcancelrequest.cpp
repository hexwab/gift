/***************************************************************************
                          giftcancelrequest.cpp  -  description
                             -------------------
    begin                : Sun Apr 7 2002
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

#include "giftcancelrequest.h"
#include <xmltag.h>
#include <iostream>

giFTCancelRequest::giFTCancelRequest(QString _name, unsigned long _id){
   name = _name;
   id = _id;
}

giFTCancelRequest::~giFTCancelRequest(){
}

XmlTag* giFTCancelRequest::xml(){
  XmlTag* tag = new XmlTag();
  tag->setName(name);
  tag->append("id", QString::number(id));
  tag->append("action", "cancel");
  return(tag);
}