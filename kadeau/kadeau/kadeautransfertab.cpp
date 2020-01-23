/***************************************************************************
                          kadeautransfertab.cpp  -  description
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

#include "kadeautransfertab.h"
#include "kadeautransferviewitem.h"
#include "kadeautransferviewsource.h"
#include "gifttransferevent.h"
#include "giftconnection.h"
#include "gifttransfersource.h"
#include "gifttransferdownloadevent.h"
#include "gifttransferuploadevent.h"
#include <qsplitter.h>
#include <qlabel.h>
#include <qpopupmenu.h>
#include <qvbox.h>
#include <iostream>
#include <kglobal.h>
#include <kiconloader.h>

KadeauTransferTab::KadeauTransferTab(QWidget* parent, const char *name,
                                     giFTConnection* gcn):KadeauTab(parent, name, gcn) {
  QSplitter* splitter = new QSplitter(this);
  splitter->setOrientation(QSplitter::Vertical);

  KIconLoader *loader = KGlobal::iconLoader();
  setMinimumWidth(600);
  QVBox* downbox = new QVBox(splitter);
  //QLabel* downlabel=
  new QLabel("Downloads", downbox);
  downlist = new QListView(downbox);	
  downlist->addColumn( "Name" );
  downlist->setColumnWidthMode(0, QListView::Manual);
  downlist->setColumnWidth(0, 200);
  downlist->addColumn( "Size" );
  downlist->addColumn( "Transferred");
  downlist->addColumn( "Bandwidth");
  downlist->addColumn( "Progress");
  downlist->setColumnWidth(4, 100);
  downlist->setAllColumnsShowFocus(TRUE);
  downmenu = new QPopupMenu(downlist);
  downmenu->insertItem(loader->loadIcon("stop", KIcon::Small),
                       "Cancel Download", this, SLOT(cancelDownload()));
  downmenu->insertItem(loader->loadIcon("find", KIcon::Small),
                        "Source Search", this, SLOT(sourceSearch()));
  downmenu->insertItem(loader->loadIcon("trashcan_full", KIcon::Small),
                        "Clear Finished", this, SLOT(clearFinished()));
  downbrowseusermenu = new QPopupMenu(downlist);
  downmenu->insertItem(loader->loadIcon("folder", KIcon::Small),
                        "Browse User", downbrowseusermenu); // Submenu



  downsourcemenu = new QPopupMenu(downlist);
  downsourcemenu->insertItem(loader->loadIcon("folder", KIcon::Small),
                              "Browse User", this, SLOT(browse())  );



  connect(downlist, SIGNAL(rightButtonPressed( QListViewItem*, const QPoint&, int)),
          this, SLOT(downloadPopup(QListViewItem*, const QPoint&, int)));

  QVBox* upbox = new QVBox(splitter);
  //QLabel* uplabel =
  new QLabel("Uploads", upbox);
  uplist = new QListView(upbox);
  uplist->addColumn( "Name" );
  uplist->setColumnWidthMode(0, QListView::Manual);
  uplist->setColumnWidth(0, 200);
  uplist->addColumn( "Size" );
  uplist->addColumn( "Transferred");
  uplist->addColumn( "Bandwidth");
  uplist->addColumn( "Progress");
  uplist->setColumnWidth(4, 100);
  uplist->setAllColumnsShowFocus(TRUE);

  upmenu = new QPopupMenu(uplist);
  upmenu->insertItem(loader->loadIcon("stop", KIcon::Small),
                      "Cancel Upload", this, SLOT(cancelUpload()));
  upmenu->insertItem(loader->loadIcon("folder", KIcon::Small),
                     "Browse user", this, SLOT(browse()) );
  upmenu->insertItem(loader->loadIcon("trashcan_full", KIcon::Small),
                     "Clear Finished", this, SLOT(clearFinished()));
  connect(uplist, SIGNAL(rightButtonPressed( QListViewItem*, const QPoint&, int)),
          this, SLOT(uploadPopup(QListViewItem*, const QPoint&, int)) );


  splitter->setResizeMode( upbox, QSplitter::KeepSize);
  splitter->show();

  connect(gcn, SIGNAL( newTransfer(giFTTransferEvent*) ),
          this, SLOT( addTransfer(giFTTransferEvent*) )  );
  connect(gcn, SIGNAL( socketError(int) ),
          this, SLOT( clearup() ) );
  connect(gcn, SIGNAL( disconnected() ),
          this, SLOT( clearup() ) );
}


KadeauTransferTab::~KadeauTransferTab(){
}


void KadeauTransferTab::addTransfer(giFTTransferEvent* trans)
{
   KadeauTransferViewItem* item;
   giFTTransferDownloadEvent* dl = dynamic_cast<giFTTransferDownloadEvent*>(trans);
   if(dl){
     item = new KadeauTransferViewItem(downlist, trans);
     connect(dl, SIGNAL(sourceAdded(giFTTransferSource*)),
             this, SLOT(sourceAdded(giFTTransferSource*))  );
     connect(dl, SIGNAL(sourceDeleted(giFTTransferSource*)),
             this, SLOT(sourceDeleted(giFTTransferSource*)) );	
   }else{
     item = new KadeauTransferViewItem(uplist, trans);

   }
   connect(trans, SIGNAL(changed(unsigned long)),
           this, SLOT(transferChanged(unsigned long))  );
   connect(trans, SIGNAL(finished(unsigned long)),
           this, SLOT(transferFinished(unsigned long)) );
   items.insert( trans->id(), item);
   transfers.insert( trans->id(), trans);
}

void KadeauTransferTab::transferChanged( unsigned long id )
{
    QListViewItem* item = items[id];
    item->repaint();
}

void KadeauTransferTab::transferFinished( unsigned long id ){
   KadeauTransferViewItem* item = items[id];
   if(item){
      QString msg;
      msg.append("Transfer of ").append(item->name()).append(" completed");
      setStatus(msg);
  }
}

void KadeauTransferTab::sourceAdded(giFTTransferSource* source){
  KadeauTransferViewItem* parent = items[source->id()];
  KadeauTransferViewSource* item = new KadeauTransferViewSource(parent, source, parent->transfer->size());

  sources.insert(source->href(), item);
}

void KadeauTransferTab::sourceDeleted(giFTTransferSource* source){
  QListViewItem* sourceview = sources.find(source->href());
  if (sourceview){
    delete sourceview;
  }
}

void KadeauTransferTab::downloadPopup(QListViewItem* item, const QPoint & point, int col){
  current = NULL;
  user = "";
  KadeauTransferViewItem* transview =  dynamic_cast<KadeauTransferViewItem*>(item);
  if (!item)
      return;

  if(transview){
     current = transview->transfer;
     giFTTransferDownloadEvent* dl = dynamic_cast<giFTTransferDownloadEvent*>(current);
     if(dl){
        // make the browse user sub menu.
        downbrowseusermenu->clear();
        browseusers.clear();
        KIconLoader *loader = KGlobal::iconLoader();
        QDictIterator<giFTTransferSource> *itptr = dl->sourceIterator();
        QDictIterator<giFTTransferSource>& it = *itptr;
        browseusers.resize(it.count());
        int index = 0;
        for(; it.current(); ++it){
            giFTTransferSource* source = it.current();
            int id = downbrowseusermenu->insertItem(loader->loadIcon("connect_established", KIcon::Small),
                                                    source->user(), this, SLOT(browse(int)) );
            downbrowseusermenu->setItemParameter(id, index);
            browseusers.insert(index,source);
            index++;
        }
        delete(itptr);
        downmenu->popup(point);
     }

  }else{
     // must be a source.
     user = item->text(0);
     downsourcemenu->popup(point);
  }
}

void KadeauTransferTab::uploadPopup(QListViewItem* item, const QPoint & point, int col){
  current = NULL;
  KadeauTransferViewItem* transview =  dynamic_cast<KadeauTransferViewItem*>(item);
  if(transview){
     current = transview->transfer;
     if(current){
        giFTTransferUploadEvent* upload = dynamic_cast<giFTTransferUploadEvent*> (current);
        if(upload){
          user = upload->user();
          upmenu->popup(point);
        }
     }
  }
}

void KadeauTransferTab::cancelTransfer(giFTTransferEvent* trans){
  trans->cancel();
}

void KadeauTransferTab::cancelUpload(){
  cancelTransfer(current);
}

void KadeauTransferTab::browse(){
  emit browseUser(user);
}

void KadeauTransferTab::browse(int index){
  giFTTransferSource* source = browseusers[index];
  if(source){
     emit browseUser(source->user());
  }
}


void KadeauTransferTab::cancelDownload(){
  cancelTransfer(current);
}



void KadeauTransferTab::sourceSearch(){
  giFTTransferDownloadEvent* dl = dynamic_cast<giFTTransferDownloadEvent*>(current);
  if(dl && !dl->isSearching())
      dl->sourceSearch();
}

void KadeauTransferTab::clearFinished(){
     QIntDictIterator<giFTTransferEvent> it(transfers);
     while(it.current()){
         giFTTransferEvent* transfer = it.current();
         if(transfer->isFinished()){
            // get rid of the item.
            delete items.take(transfer->id());
            // This transfer need not be in our dictionary.
            transfers.take(transfer->id());
            //tell connection we don't care about this transfer anymore.
            _gcn->dontCare(transfer);
         }else{
            ++it;
         }
     }
}

void KadeauTransferTab::clearup(){
   // get rid of all the items everywhere.
   // clear out transfers
   QIntDictIterator<giFTTransferEvent> itt(transfers);

   // All the contents will be deleted by the connection.
   transfers.clear();


   // Items

   QIntDictIterator<KadeauTransferViewItem> itdl(items);

   for(; itdl.current(); ++itdl){
       delete itdl.current();
   }
   items.clear();


   // Sources

   QDictIterator<QListViewItem> itdls(sources);

   for(; itdls.current(); ++itdls){
       delete itdl.current();
   }
   sources.clear();





}

