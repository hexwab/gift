/***************************************************************************
                          giftsearchitem.h  -  description
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

#ifndef GIFTSEARCHITEM_H
#define GIFTSEARCHITEM_H

#include <qobject.h>

/**
  *@author Robert Wittams
  */

class XmlTag;

class giFTSearchItem : public QObject  {
Q_OBJECT
private:
  QString _key;  // cache;
  QString _user;
  QString _node;
  QString _href;
  QString _hash;
  QString _availability;
  unsigned long _size;
  unsigned long _hits;
public:
  giFTSearchItem(XmlTag* tag);
  ~giFTSearchItem();
  void addHit();
  /*key is currently hash + length */


  const QString& href(){return _href;}
  const QString& user(){return _user;}
  const QString& node(){return _node;}
  const QString& key(){return _key;}
  const QString& hash(){return _hash;}
  const QString& availability(){return _availability;}
  const unsigned long& size(){return _size;}
  const unsigned long& hits(){return _hits;}
signals:
  void hitAdded(int hits);
};

#endif
