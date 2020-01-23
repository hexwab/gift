/***************************************************************************
                          giftstatsevent.h  -  description
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

#ifndef GIFTSTATSEVENT_H
#define GIFTSTATSEVENT_H

#include <giftevent.h>
#include <qlist.h>

/**
  *@author Robert Wittams
  */

class giFTStatsEvent : public giFTEvent  {
Q_OBJECT
public:
  struct ProtocolInfo{
        QString protocol; // protocol name
        QString status;
        unsigned long users;
        unsigned long files;
        unsigned long size;
  };
private:
  QList<ProtocolInfo> protocols;
public:
  giFTStatsEvent(unsigned long idno);
  ~giFTStatsEvent();
  void handleXml(XmlTag* xml);
signals:
  void newProtocol(giFTStatsEvent::ProtocolInfo* info);
};

#endif
