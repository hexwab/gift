/***************************************************************************
                          kadeausearchtab.h  -  description
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

#ifndef KADEAUSEARCHTAB_H
#define KADEAUSEARCHTAB_H

#include "kadeautab.h"
#include "kadeauqueryview.h"
#include "kadeaubrowseview.h"
#include <qlist.h>
#include <qvector.h>
#include <qpixmap.h>


/**
  *@author Robert Wittams
  */
class KadeauSearchViewItem;
class giFTConnection;
class giFTEvent;
class giFTSearchEvent;
class giFTSearchItem;
class giFTTransferRequest;
class QLineEdit;
class QListView;
class QListViewItem;
class QString;
class QWidgetStack;
class QButtonGroup;
class QRadioButton;
class QTabWidget;
class QComboBox;
struct KadeauQueryType;
struct KadeauQuerySpec;


class KadeauSearchTab : public KadeauTab  {
Q_OBJECT
private:
  QLineEdit* querybox;
 // QWidgetStack* extra;
  QComboBox* querycombo;

  QButtonGroup* realm;
  QPushButton* go;

  QTabWidget* viewtab;

  QVector<KadeauQueryType> types;
public:
  KadeauSearchTab(QWidget* parent, const char *name, giFTConnection* gcn);
  ~KadeauSearchTab();
  void startQuery(const KadeauQuerySpec& spec);
public slots:
  void browseUser(const QString& user);
  void startSearch();
  void searchComplete(int numKeys, int numHits);
  void transferRequested(giFTTransferRequest* item, const QString& name );
  void doDontcare(giFTSearchEvent* ev);
signals:
  void transferRequested();
};

#endif
