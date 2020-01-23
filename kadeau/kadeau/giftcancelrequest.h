/***************************************************************************
                          giftcancelrequest.h  -  description
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

#ifndef GIFTCANCELREQUEST_H
#define GIFTCANCELREQUEST_H

#include <giftrequest.h>

/**
  *@author Robert Wittams
  */

class giFTCancelRequest : public giFTRequest  {
Q_OBJECT
private:
  QString name;
  unsigned long id;
public: 
	giFTCancelRequest(QString name, unsigned long id);
	~giFTCancelRequest();
  XmlTag* xml();
};

#endif
