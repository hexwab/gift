/***************************************************************************
                          giftattachrequest.h  -  description
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

#ifndef GIFTATTACHREQUEST_H
#define GIFTATTACHREQUEST_H

#include <giftrequest.h>

/**
  *@author Robert Wittams
  */

class giFTAttachRequest : public giFTRequest  {
Q_OBJECT
private:
  QString user, client, version;

public: 
  giFTAttachRequest();
  giFTAttachRequest(QString user, QString client, QString version);
  ~giFTAttachRequest();
  XmlTag* xml();
};

#endif
