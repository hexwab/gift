/***************************************************************************
                          kadeauqueryview.h  -  description
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

#ifndef KADEAUQUERYVIEW_H
#define KADEAUQUERYVIEW_H

#include <kadeausearchview.h>
#include <qdict.h>
#include <qptrdict.h>
#include <qlist.h>


/**
  *@author Robert Wittams
  */

class QPopupMenu;

class KadeauQueryView : public KadeauSearchView  {
Q_OBJECT
private:
   QDict<KadeauSearchViewItem> keys2items;
   QPtrDict<QString> items2keys;
   QPopupMenu *menu, * sourcemenu, *browseusermenu;
   QString user;
   QList<giFTSearchItem> *sources;
public:
  KadeauQueryView(QWidget* parent,const KadeauQuerySpec& spec);
  ~KadeauQueryView();

public slots:
  void addKey(const QString& key,  giFTSearchItem* si);
  void addSource(const QString& key, giFTSearchItem* si);
  void itemDoubleClicked(QListViewItem *);
  void queryPopup(QListViewItem* item, const QPoint & point, int col);
  void browse();
  void browse(int index);
};

#endif
