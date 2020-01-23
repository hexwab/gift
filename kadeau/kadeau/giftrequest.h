/***************************************************************************
                          giftrequest.h  -  description
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

#ifndef GIFTREQUEST_H
#define GIFTREQUEST_H

#include <qobject.h>

/**
  *@author Robert Wittams
  */

class giFTEvent;
class XmlTag;

class giFTRequest : public QObject {
Q_OBJECT

public: 
  giFTRequest();
  ~giFTRequest();
  virtual XmlTag* xml() = 0;  // make xml appropriate to send to the server.
};

#endif
