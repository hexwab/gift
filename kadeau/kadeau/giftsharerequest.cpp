/***************************************************************************
                          giftsharerequest.cpp  -  description
                             -------------------
    begin                : Sat Apr 13 2002
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

#include "giftsharerequest.h"
#include "xmltag.h"

giFTShareRequest::giFTShareRequest(Action action){
_action = action;
}
giFTShareRequest::~giFTShareRequest(){
}

XmlTag * giFTShareRequest::xml(){
    XmlTag* tag = new XmlTag();
    tag->setName("share");

    QString actval;
    switch(_action){
    case Sync:
      actval = "sync";
      break;
    case Hide:
      actval = "hide";
      break;
    case Show:
      actval = "show";
      break;
    default:
      break;
    }

    tag->setValue("action", actval);

    return tag;
}
