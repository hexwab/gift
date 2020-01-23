/***************************************************************************
                          kadeaubrowseview.cpp  -  description
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

#include "kadeaubrowseview.h"
#include "kadeauutil.h"
#include "gifttransferrequest.h"
#include "giftsearchitem.h"
#include "giftsearchevent.h"

#include <qlistview.h>
#include <kglobal.h>
#include <kiconloader.h>
#include <qurl.h>
#include <qlabel.h>


KadeauBrowseView::KadeauBrowseView(QWidget* parent,const KadeauQuerySpec& _spec):
                  KadeauSearchView(parent, _spec){

  iconlabel->setPixmap( spec.type->icon);
  label->setText( tr("Browsing user <i>%1</i>").arg(spec.query) );

  sr = new QListView(this);
  sr->addColumn(tr("Name"));
  sr->setColumnWidthMode(0, QListView::Manual);
  sr->setColumnWidth(0, 250);
  sr->addColumn(tr("Size"));
  sr->setColumnWidth(1,80);
  sr->setAllColumnsShowFocus(TRUE);
  sr->setRootIsDecorated(TRUE); 	
  sr->setMinimumSize(300, 150);
  sr->setShowSortIndicator(TRUE);
  sr->setSorting(1,TRUE);

  root = NULL;

  connect( sr, SIGNAL(doubleClicked(QListViewItem *)),
           this, SLOT(itemDoubleClicked(QListViewItem *) ) );
}

KadeauBrowseView::~KadeauBrowseView(){

}



QListViewItem*  KadeauBrowseView::getParent(QString dirpath, QString rootname){

      QListViewItem* parent = items[dirpath];
      if (!parent){
          int index = dirpath.findRev('/');
          QString direlement = dirpath.mid(index + 1);
          QString parentdirpath = dirpath.left(index);
          if(parentdirpath != ""){
            QListViewItem* grandparent = getParent(parentdirpath, rootname);
            parent = new QListViewItem(grandparent,direlement);
            KIconLoader *loader = KGlobal::iconLoader();
            parent->setPixmap(0, loader->loadIcon("folder", KIcon::Small));

            items.insert(dirpath, parent);
          }else{
             parent = root;
          }
      }
      return parent;
}

void  KadeauBrowseView::addKey(const QString& key, giFTSearchItem* si){

  QUrl* url = new QUrl(si->href());
  KIconLoader *loader = KGlobal::iconLoader();

  if(!root){
      root = new QListViewItem(sr, url->host());
      root->setPixmap(0, loader->loadIcon("connect_established", KIcon::Small));
      root->setOpen(TRUE);
  }


  QListViewItem* parent = getParent(url->dirPath(),url->host());

  QListViewItem* item =
  new QListViewItem(parent, url->fileName(), KadeauUtil::convert_size(si->size()));

  item->setPixmap(0, loader->loadIcon("mime_empty", KIcon::Small));

  list2search.insert(item, si);
  delete url;
}

void KadeauBrowseView::addSource(const QString& key, giFTSearchItem* si){

}

void KadeauBrowseView::itemDoubleClicked(QListViewItem* item){
giFTSearchItem* si = list2search[item];

  if(si){
      QList<giFTSearchItem> *sources = new QList<giFTSearchItem>;
      sources->append(si);
      giFTTransferRequest* trans =
            new  giFTTransferRequest( si->size() , si->hash()  , item->text(0), sources);
      emit transferRequested(trans, item->text(0));
 }
}