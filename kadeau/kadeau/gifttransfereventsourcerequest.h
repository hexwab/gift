/***************************************************************************
                          gifttransfereventsourcerequest.h  -  description
                             -------------------
    begin                : Wed Apr 3 2002
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

#ifndef GIFTTRANSFEREVENTSOURCEREQUEST_H
#define GIFTTRANSFEREVENTSOURCEREQUEST_H

#include <giftrequest.h>

/**
  *@author Robert Wittams
  */

class giFTSearchItem;

class giFTTransferEventSourceRequest : public giFTRequest  {	
  QString user;
  QString href;
  bool add; // whether it is an addsource or delsource
  unsigned int id;
public:
  giFTTransferEventSourceRequest(unsigned int id, giFTSearchItem* item, bool add = TRUE);
  ~giFTTransferEventSourceRequest();
  XmlTag* xml();
};

#endif
