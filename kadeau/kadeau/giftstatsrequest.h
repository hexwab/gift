/***************************************************************************
                          giftstatsrequest.h  -  description
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

#ifndef GIFTSTATSREQUEST_H
#define GIFTSTATSREQUEST_H

#include <gifteventrequest.h>

/**
  *@author Robert Wittams
  */

class giFTStatsRequest : public giFTEventRequest  {
Q_OBJECT
public:
  giFTStatsRequest();
  ~giFTStatsRequest();
  giFTEvent* makeEvent(XmlTag* xml);
  XmlTag* xml();	
};

#endif
