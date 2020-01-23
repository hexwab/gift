/***************************************************************************
                          gifttransferuploadevent.h  -  description
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

#ifndef GIFTTRANSFERUPLOADEVENT_H
#define GIFTTRANSFERUPLOADEVENT_H

#include <gifttransferevent.h>

/**
  *@author Robert Wittams
  */

class giFTTransferUploadEvent : public giFTTransferEvent  {
Q_OBJECT
private:
  QString _href;
  QString _user;
public:
  const QString& href(){return _href;}
  const QString& user(){return _user;}
  giFTTransferUploadEvent(unsigned long id, XmlTag* tag);
  ~giFTTransferUploadEvent();
};



#endif
