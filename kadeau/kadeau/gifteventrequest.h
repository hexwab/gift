/***************************************************************************
                          gifteventrequest.h  -  description
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

#ifndef GIFTEVENTREQUEST_H
#define GIFTEVENTREQUEST_H

#include <giftrequest.h>

/**
  *@author Robert Wittams
  */

class giFTEventRequest : public giFTRequest  {
Q_OBJECT
public:
  giFTEventRequest();
  ~giFTEventRequest();	

  virtual giFTEvent* makeEvent(XmlTag* tag) = 0; // Make an appropriate event from this server response.
                                                 // Will take possesion of the xml tag.
signals:
  void madeEvent(giFTEvent*);
};

#endif
