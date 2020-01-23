/***************************************************************************
                          kadeaubrowseview.h  -  description
                             -------------------
    begin                : Tue Apr 16 2002
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

#ifndef KADEAUBROWSEVIEW_H
#define KADEAUBROWSEVIEW_H

#include <kadeausearchview.h>

/**
  *@author Robert Wittams
  */

class KadeauBrowseView : public KadeauSearchView  {
Q_OBJECT
private:
  QDict<QListViewItem> items;
  QPtrDict<giFTSearchItem> list2search;
  QListViewItem* root;
  QListViewItem* getParent(QString dirpath, QString rootname);
public:
	KadeauBrowseView(QWidget* parent, const KadeauQuerySpec& spec);
	~KadeauBrowseView();
public slots:
  void addKey(const QString& key,  giFTSearchItem* si);
  void addSource(const QString& key, giFTSearchItem* si);
  void itemDoubleClicked(QListViewItem *);
};

#endif
