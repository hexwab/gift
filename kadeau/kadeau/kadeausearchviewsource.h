/***************************************************************************
                          kadeausearchsourceview.h  -  description
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

#ifndef KADEAUSEARCHVIEWSOURCE_H
#define KADEAUSEARCHVIEWSOURCE_H

#include <qwidget.h>
#include <qlistview.h>

/**
  *@author Robert Wittams
  */

class giFTSearchItem;

class KadeauSearchViewSource : public QListViewItem  {
private:
  QColorGroup* newcg;
  giFTSearchItem* item; // Source this represents
public: 
  KadeauSearchViewSource(QListViewItem *parent, giFTSearchItem* item);
  ~KadeauSearchViewSource();
  QString text ( int col) const;
  void paintCell(QPainter *, const QColorGroup & cg, int column, int width, int alignment);
};

#endif
