/***************************************************************************
                          gifttransferrequest.h  -  description
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

#ifndef GIFTTRANSFERREQUEST_H
#define GIFTTRANSFERREQUEST_H

#include <gifteventrequest.h>
#include <qlist.h>

/**
  *@author Robert Wittams
  */

/* Request for a download. */
class giFTSearchItem;

class giFTTransferRequest : public giFTEventRequest  {
Q_OBJECT
private:
  QList<giFTSearchItem>* sources;
  unsigned long size;
  QString hash, filename;
public: 
  giFTTransferRequest( int sz, QString hsh, QString fname, QList<giFTSearchItem>* sources);
  ~giFTTransferRequest();
  giFTEvent* makeEvent(XmlTag* xml);
  XmlTag* xml();
};

#endif
