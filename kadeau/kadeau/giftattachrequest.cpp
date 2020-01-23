/***************************************************************************
                          giftattachrequest.cpp  -  description
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

#include "giftattachrequest.h"
#include "xmltag.h"
#include <iostream>

giFTAttachRequest::giFTAttachRequest(QString _u, QString _c, QString _v){
  user = _u;
  client = _c;
  version = _v;

}
giFTAttachRequest::~giFTAttachRequest(){
}


XmlTag* giFTAttachRequest::xml(){
  XmlTag* tag = new XmlTag();
  tag->setName("attach");
  tag->append("profile", user);
  tag->append("client", client);
  tag->append("version", version);
  return(tag);
}