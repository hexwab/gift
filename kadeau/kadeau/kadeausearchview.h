/***************************************************************************
                          kadeausearchview.h  -  description
                             -------------------
    begin                : Mon Apr 15 2002
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

#ifndef KADEAUSEARCHVIEW_H
#define KADEAUSEARCHVIEW_H

#include <qvbox.h>
#include <qdict.h>
#include <qptrdict.h>
#include <kadeauquery.h>

class KadeauSearchViewItem;
class giFTEvent;
class giFTSearchItem;
class giFTSearchEvent;
class giFTTransferRequest;
class QListView;
class QListViewItem;
class QString;
class QLabel;
/**
  *@author Robert Wittams
  */
class KadeauSearchView: public QVBox{
Q_OBJECT
protected:
    QLabel* label;
    QLabel* iconlabel;
    giFTSearchEvent* _sev;
    QListView* sr;
    KadeauQuerySpec spec;
public:
    KadeauSearchView(QWidget* parent, const KadeauQuerySpec& spec);
   ~KadeauSearchView();
public slots:
   void virtual addKey(const QString& key,  giFTSearchItem* si) = 0;
   void virtual addSource(const QString& key, giFTSearchItem* si) =0;
   void deleteThis();
   void gotEvent(giFTEvent* ev);
signals:
   void transferRequested(giFTTransferRequest* trans, const QString& name);
   void searchComplete(int, int);
   void doDontcare(giFTSearchEvent* ev);
   void browseUser(const QString& user);
};

#endif
