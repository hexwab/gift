/***************************************************************************
                          kadeautransfertab.h  -  description
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

#ifndef KADEAUTRANSFERTAB_H
#define KADEAUTRANSFERTAB_H

#include <kadeautab.h>
#include <qlist.h>
#include <qlistview.h>
#include <qintdict.h>
#include <qdict.h>
#include <qvector.h>

/**
  *@author Robert Wittams
  */

class giFTConnection;
class giFTTransferEvent;
class giFTTransferSource;
class QPopupMenu;
class KadeauTransferViewItem;


class KadeauTransferTab : public KadeauTab {
  Q_OBJECT
private:
  QListView* uplist;
  QListView* downlist;

  // menu stuff.
  QPopupMenu* upmenu;
	QPopupMenu* downmenu;
	QPopupMenu* downsourcemenu;
  QPopupMenu* downbrowseusermenu;
  QVector<giFTTransferSource> browseusers; // right click browse users.
  giFTTransferEvent* current; // The transfer event which is currently selected via a right click or whatever.
  QString user; // The user currently under right click.


  QIntDict<KadeauTransferViewItem> items;
  QDict<QListViewItem> sources;
  QIntDict<giFTTransferEvent> transfers;

public: 
  KadeauTransferTab(QWidget* parent = 0, const char *name=0,giFTConnection* gcn = NULL);
  ~KadeauTransferTab();
public slots:
  void addTransfer(giFTTransferEvent* trans);
  void transferChanged(unsigned long id);
  void transferFinished( unsigned long id );
  void sourceAdded(giFTTransferSource*);
  void sourceDeleted(giFTTransferSource*);
  void downloadPopup(QListViewItem*, const QPoint&, int);	
  void uploadPopup(QListViewItem*, const QPoint&, int);
  void browse();
  void browse(int index);
  void cancelTransfer(giFTTransferEvent*);
  void cancelUpload();
  void cancelDownload();
  void clearFinished();
  void sourceSearch();
  void clearup();
};

#endif
