/***************************************************************************
                          giftsearchrequest.h  -  description
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

#ifndef GIFTSEARCHREQUEST_H
#define GIFTSEARCHREQUEST_H

#include <gifteventrequest.h>

/**
  *@author Robert Wittams
  */

class giFTSearchRequest : public giFTEventRequest  {
Q_OBJECT
private:
  QString query,  exclude, protocol,
          type, realm, csize, ckbps;
public: 
  giFTSearchRequest(QString q ,
                    QString e = "",
                    QString p = "",
                    QString t = "",
                    QString r = "",
                    QString cs = "" ,
                    QString cb = "" );
  ~giFTSearchRequest();
  giFTEvent* makeEvent(XmlTag* xml);
  XmlTag* xml();
};

#endif
