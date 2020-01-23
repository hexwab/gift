/***************************************************************************
                          giftsharerequest.h  -  description
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

#ifndef GIFTSHAREREQUEST_H
#define GIFTSHAREREQUEST_H

#include <giftrequest.h>

/**
  *@author Robert Wittams
  */

class giFTShareRequest : public giFTRequest  {
Q_OBJECT
public:
  enum Action{
  Sync,
  Hide,
  Show};
private:
  Action _action;
public:
	giFTShareRequest(Action action);
	~giFTShareRequest();
  /** No descriptions */
  XmlTag * xml();
};

#endif
