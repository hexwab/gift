/***************************************************************************
                          giftsearchevent.h  -  description
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

#ifndef GIFTSEARCHEVENT_H
#define GIFTSEARCHEVENT_H

#include <giftevent.h>
#include <qdict.h>

/**
  *@author Robert Wittams
  */

class giFTSearchItem ;

class giFTSearchEvent : public giFTEvent {
Q_OBJECT
private:
  int numKeys, numHits;
public:
  QDict< QDict<giFTSearchItem> >  items;

  giFTSearchEvent( unsigned long idno);
  ~giFTSearchEvent();
  void handleXml(XmlTag* xml);
signals:
  void newKey( const QString& key, giFTSearchItem* si);
  void newSource( const QString& key, giFTSearchItem* si);
  void searchComplete(int numKeys, int numHits);
};

#endif
