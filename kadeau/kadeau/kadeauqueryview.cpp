/***************************************************************************
                          kadeauqueryview.cpp  -  description
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

#include "kadeauqueryview.h"
#include "kadeausearchviewitem.h"
#include "kadeausearchviewsource.h"
#include "gifttransferrequest.h"
#include "giftsearchitem.h"
#include <iostream>
#include <qlabel.h>
#include <qpopupmenu.h>
#include <kglobal.h>
#include <kiconloader.h>

KadeauQueryView::KadeauQueryView(QWidget* parent, const KadeauQuerySpec& _spec):
                  KadeauSearchView(parent, _spec){
  KIconLoader *loader = KGlobal::iconLoader();
  iconlabel->setPixmap( spec.type->icon);
  label->setText( tr("Results for <i>%1</i> with keywords <i>%2</i>").arg(spec.type->display).arg(spec.query) );

  sr = new QListView(this);
  sr->addColumn("Name");
  sr->setColumnWidthMode(0, QListView::Manual);
  sr->setColumnWidth(0, 250);
  sr->addColumn("Size");
  sr->setColumnWidth(1,80);
  sr->addColumn("Hits");
  sr->setColumnWidth(2,80);
  sr->setAllColumnsShowFocus(TRUE);
  sr->setRootIsDecorated(TRUE);
  sr->setMinimumSize(300, 150);
  sr->setShowSortIndicator(TRUE);
  sr->setSorting(2,FALSE);

  connect( sr, SIGNAL(doubleClicked(QListViewItem *)),
           this, SLOT(itemDoubleClicked(QListViewItem *) ) );

  connect( sr, SIGNAL(rightButtonPressed(QListViewItem*, const QPoint&, int)),
          this, SLOT(queryPopup(QListViewItem*, const QPoint&, int)));

  menu = new  QPopupMenu(sr);
  browseusermenu = new QPopupMenu(sr);
  menu->insertItem(loader->loadIcon("folder", KIcon::Small),
                        "Browse User", browseusermenu); // Submenu

  sourcemenu = new QPopupMenu(sr);
  sourcemenu->insertItem(loader->loadIcon("folder", KIcon::Small),
                     "Browse user", this, SLOT(browse()) );

}

KadeauQueryView::~KadeauQueryView(){
}


void KadeauQueryView::addKey(const QString& key,  giFTSearchItem* si){
  KadeauSearchViewItem* item =
  new KadeauSearchViewItem(sr, key);

     // add to dict.
  keys2items.insert( key, item);
}

void KadeauQueryView::addSource(const QString& key, giFTSearchItem* si){

  //look up parent in dict.
  KadeauSearchViewItem* vi = keys2items[key];

  if( vi != NULL){
     new KadeauSearchViewSource(vi, si);
     vi->addSource(si);
  }else{
     cerr << "Couldn't find parent" << endl;
  }
}

void KadeauQueryView::itemDoubleClicked(QListViewItem* item){
   KadeauSearchViewItem* vi = dynamic_cast<KadeauSearchViewItem*>(item);

   if(vi){
      giFTTransferRequest* trans =
            new  giFTTransferRequest( vi->size , vi->hash  , vi->best_name, vi->sources);
      emit transferRequested(trans, vi->best_name);
   }
}

void KadeauQueryView::queryPopup(QListViewItem* item, const QPoint & point, int col){
  user = "";
  KadeauSearchViewItem* svi = dynamic_cast<KadeauSearchViewItem*> (item) ;

  if(svi){
      KIconLoader *loader = KGlobal::iconLoader();
      browseusermenu->clear();
      sources = svi->sources;
      QListIterator<giFTSearchItem> it(*sources);
      int index = 0;
      for(; it.current(); ++it){
         int id = browseusermenu->insertItem(loader->loadIcon("connect_established", KIcon::Small),
                                                 it.current()->user(), this, SLOT(browse(int)) );
         browseusermenu->setItemParameter(id, index);
         index++;
      }
      menu->popup(point);
  }else{
     if(item){
        user = item->text(0);
        sourcemenu->popup(point);
     }
  }
}

void KadeauQueryView::browse(){
  emit browseUser(user);
}

void  KadeauQueryView::browse(int index){
  giFTSearchItem* item = sources->at(index);
  if(item){
    user = item->user();
    browse();
  }
}
