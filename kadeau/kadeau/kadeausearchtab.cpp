/***************************************************************************
                          kadeausearchtab.cpp  -  description
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



#include "kadeausearchtab.h"
#include "giftconnection.h"
#include "giftsearchrequest.h"
#include "giftsearchevent.h"
#include "giftsearchitem.h"
#include "gifttransferrequest.h"
#include "xmltag.h"

#include <qlayout.h>
#include <qsplitter.h>
#include <qlabel.h>
#include <qlineedit.h>
#include <qpushbutton.h>
#include <qradiobutton.h>
#include <qcombobox.h>
#include <qlistview.h>
#include <qwidgetstack.h>
#include <qbuttongroup.h>
#include <qtabwidget.h>
#include <iostream>
#include <kglobal.h>
#include <kiconloader.h>


#define MAX_TAB_CHARS 20

KadeauSearchTab::KadeauSearchTab(QWidget* parent, const char *name,
  giFTConnection* gcn): KadeauTab(parent, name, gcn) {

  KIconLoader *loader = KGlobal::iconLoader();
  QSplitter* splitter = new QSplitter(this);
  splitter->setOrientation(Qt::Vertical);
  QWidget* searchpanel = new QWidget(splitter);
  //searchpanel->setMinimumSize(100, 150);
  searchpanel->setMaximumHeight(50);
  QGridLayout *grid = new QGridLayout( searchpanel, 2, 3);


  QLabel* querylabel = new QLabel(searchpanel);
  querylabel->setText(tr("Search for "));
  querylabel->setFixedHeight( 20 );

  QLabel* keylabel =   new QLabel(searchpanel);
  keylabel->setText(tr("with keywords: "));
  keylabel->setFixedHeight( 20 );


  querybox = new QLineEdit(searchpanel);

  querycombo = new QComboBox(searchpanel);

  types.resize(7);

  types.insert(0, new KadeauQueryType(tr("everything"), "", "",
               loader->loadIcon("find",KIcon::Small) ));
  types.insert(1, new KadeauQueryType(tr("audio"),      "", "audio",
               loader->loadIcon("sound",KIcon::Small) ));
  types.insert(2, new KadeauQueryType(tr("video"),      "", "video",
               loader->loadIcon("video",KIcon::Small)));
  types.insert(3, new KadeauQueryType(tr("images"),     "", "image",
               loader->loadIcon("image",KIcon::Small)));
  types.insert(4, new KadeauQueryType(tr("text documents"),       "", "text",
               loader->loadIcon("txt",KIcon::Small)));
  types.insert(5, new KadeauQueryType(tr("software"),   "", "application",
               loader->loadIcon("binary",KIcon::Small)));
  types.insert(6, new KadeauQueryType(tr("users"),       "user", "",
               loader->loadIcon("connect_established",KIcon::Small)));


  for(unsigned int i =0 ; i< types.count() ; i++){
    querycombo->insertItem(types[i]->display);
  }

  // extra = new QWidgetStack(searchpanel);


  //extra->addWidget(realm,0);
  //extra->raiseWidget(realm);

  QPushButton* go = new QPushButton(searchpanel);
  go->setPixmap( loader->loadIcon("down", KIcon::Toolbar));
  grid->addWidget(querylabel,0,0);
  grid->addWidget(querycombo,0,1);
  grid->addWidget(keylabel, 1, 0);
  grid->addWidget(querybox, 1, 1);
  grid->addWidget(go ,1, 2);

  grid->setRowStretch(0, 0);
  grid->setRowStretch(1, 0);
  grid->setColStretch(0, 0);
  grid->setColStretch(1, 1);
  grid->setColStretch(2, 0);


  viewtab = new QTabWidget(splitter);
  viewtab->setTabPosition(QTabWidget::Bottom);
  viewtab->setTabShape(QTabWidget::Triangular);

  connect( go, SIGNAL(clicked()),
           this, SLOT(startSearch()) );
  connect( querybox, SIGNAL(returnPressed()),
           this, SLOT(startSearch()) );
}


KadeauSearchTab::~KadeauSearchTab() {
}

void KadeauSearchTab::startQuery(const KadeauQuerySpec& spec){


   giFTSearchRequest* search =
            new giFTSearchRequest(spec.query, // query
                                  "", //exclude list
                                  "", //protocol
                                  spec.type->typestr, //type - user / hash/ "" are options
                                  spec.type->realmstr, // realm
                                  "", // csize
                                  ""  // ckbkps
                                  );

    _gcn->addEventRequest(search);
    KadeauSearchView* ksv;

    if( spec.type->typestr.compare("user") == 0 ){
      ksv = new KadeauBrowseView(viewtab, spec);
    }else{
      ksv = new KadeauQueryView(viewtab, spec);
    }
    QString tab =  spec.query;
    tab = (tab.length() < MAX_TAB_CHARS ) ? tab : tab.left(MAX_TAB_CHARS - 3).append("...") ;

    viewtab->addTab(ksv,spec.type->icon, tab);
    viewtab->showPage(ksv);

    connect(search, SIGNAL(madeEvent(giFTEvent*)),
            ksv, SLOT(gotEvent(giFTEvent*))   );

    connect(ksv, SIGNAL(transferRequested(giFTTransferRequest*, const QString&)),
            this, SLOT(transferRequested(giFTTransferRequest*, const QString&)) );

    connect(ksv, SIGNAL(searchComplete(int,int)),
            this,  SLOT(searchComplete(int,int)) );

    connect(ksv, SIGNAL(doDontcare(giFTSearchEvent*)),
            this,  SLOT(doDontcare(giFTSearchEvent*)) );

    connect(ksv, SIGNAL(browseUser(const QString&)),
            this,  SLOT(browseUser(const QString&)) );
}


void KadeauSearchTab::startSearch(){
    // find out which realm thing is selected.
    int index = querycombo->currentItem();

    KadeauQuerySpec qs(types[index],querybox->text());
    startQuery(qs);
}



void KadeauSearchTab::transferRequested(giFTTransferRequest* trans, const QString& name){
      _gcn->addEventRequest(trans);

      // FIXME: translatable
      QString msg ;

      msg += tr("%1 requested").arg(name);

      setStatus(msg);
      emit transferRequested();
}

void KadeauSearchTab::searchComplete(int numKeys, int numHits){
    //FIXME: translatable

    QString msg;
    msg += "Search finished - ";
    msg += QString::number(numKeys);
    msg += " unique files, ";
    msg += QString::number(numHits);
    msg += " hits." ;
    setStatus(msg);
}

void KadeauSearchTab::doDontcare(giFTSearchEvent* ev){
  _gcn->dontCare(ev);
}

void KadeauSearchTab::browseUser(const QString& userstr){
  KadeauQuerySpec qs(types[6],userstr);
  startQuery(qs);
}
