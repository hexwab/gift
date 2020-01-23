/***************************************************************************
                          kadeausearchitemview.h  -  description
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

#ifndef KADEAUSEARCHVIEWITEM_H
#define KADEAUSEARCHVIEWITEM_H

#include <qlistview.h>
#include <qlist.h>

/**
  *@author Robert Wittams
  */

class giFTSearchItem;

class KadeauSearchViewItem : public QListViewItem  {
private: 	
	bool no_name;
  int availability() const ;
  QColorGroup* newcg;
public:
  QList<giFTSearchItem> *sources;
  QString _key;
  QString hash;
  QString best_name;
  QString best_type;
  unsigned long size;



  KadeauSearchViewItem(QListView *parent, QString key );
  ~KadeauSearchViewItem();
  QString key ( int col, bool ascending) const;
  QString text ( int col) const;
  void paintCell(QPainter *, const QColorGroup & cg, int column, int width, int alignment);
  void addSource(giFTSearchItem* item);
  QString & getKey();
};

#endif
