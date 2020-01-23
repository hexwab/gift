/***************************************************************************
                          kadeautransferviewsource.h  -  description
                             -------------------
    begin                : Tue Apr 23 2002
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

#ifndef KADEAUTRANSFERVIEWSOURCE_H
#define KADEAUTRANSFERVIEWSOURCE_H

#include <qlistview.h>

/**
  *@author Robert Wittams
  */
class giFTTransferSource;

class KadeauTransferViewSource : public QListViewItem  {
private:
  giFTTransferSource* _source;
  unsigned long _transfer_size;
public: 
	KadeauTransferViewSource(QListViewItem* parent, giFTTransferSource* source,unsigned long transfer_size);
	~KadeauTransferViewSource();

  QString text(int col) const;
  QString key( int col, bool ascending) const;
  void paintCell(QPainter *, const QColorGroup & cg, int column, int width, int alignment);
};

#endif
