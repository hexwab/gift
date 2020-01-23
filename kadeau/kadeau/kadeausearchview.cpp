/***************************************************************************
                          kadeausearchview.cpp  -  description
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

#include "kadeausearchtab.h"

#include "giftsearchevent.h"
#include "giftsearchitem.h"

#include <qpushbutton.h>
#include <qlistview.h>
#include <qlabel.h>
#include <iostream>
#include <kglobal.h>
#include <kiconloader.h>

#include "kadeausearchview.h"

KadeauSearchView::KadeauSearchView(QWidget* parent,const KadeauQuerySpec& _spec):
                  QVBox(parent, _spec.query), spec(_spec){


  KIconLoader *loader = KGlobal::iconLoader();

  QHBox* box = new QHBox(this);
  iconlabel = new QLabel(box);
  label =new QLabel(box);
  QPushButton* close = new QPushButton(box);
  close->setPixmap(loader->loadIcon("stop", KIcon::Small));
  close->setFixedHeight( close->sizeHint().height() );
  close->setFixedWidth( close->sizeHint().width() );
  //close->setFlat(TRUE);
  box->setStretchFactor(label, 4);
  box->setStretchFactor(close, 0);
  connect(close, SIGNAL(clicked()),
          this, SLOT(deleteThis()) );


}

KadeauSearchView::~KadeauSearchView(){
    if(_sev){
        // Make sure we get no more signals from this search.
        _sev->disconnect(this);
        // Get the connection to look at this event and cancel it if necessary
        emit doDontcare(_sev);
    }
}




void KadeauSearchView::deleteThis(){
   delete this;
}

void  KadeauSearchView::gotEvent( giFTEvent* ev ){
    giFTSearchEvent* sev = dynamic_cast<giFTSearchEvent*>(ev) ;
    if(sev){
       _sev = sev;
       connect (sev, SIGNAL(newKey(const QString&, giFTSearchItem*)),
           this, SLOT(addKey(const QString&, giFTSearchItem*)) );

       connect (sev, SIGNAL( newSource(const QString&, giFTSearchItem*)),
           this, SLOT(addSource(const QString&, giFTSearchItem*)) );

       connect (sev, SIGNAL(searchComplete(int,int)),
           this, SIGNAL(searchComplete(int,int)) );
    }
}