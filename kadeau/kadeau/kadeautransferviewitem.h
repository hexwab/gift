/***************************************************************************
                          kadeautransferviewitem.h  -  description
                             -------------------
    begin                : Thu Apr 4 2002
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

#ifndef KADEAUTRANSFERVIEWITEM_H
#define KADEAUTRANSFERVIEWITEM_H

#include <qlistview.h>

/**
  *@author Robert Wittams
  */

class giFTTransferEvent;

class KadeauTransferViewItem : public QListViewItem  {
  QString _name;
public:
  giFTTransferEvent* transfer;
  KadeauTransferViewItem(QListView* parent, giFTTransferEvent* _transfer);
  ~KadeauTransferViewItem();
  QString& name(){return _name;}
  QString text(int col) const;
  QString key ( int col, bool ascending) const;
  void paintCell(QPainter *, const QColorGroup & cg, int column, int width, int alignment);
};

#endif
