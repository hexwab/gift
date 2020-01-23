/***************************************************************************
                          kadeausearchsourceview.cpp  -  description
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

#include "kadeausearchviewsource.h"
#include "giftsearchitem.h"
#include <kglobal.h>
#include <kiconloader.h>
#include <iostream.h>

KadeauSearchViewSource::KadeauSearchViewSource(QListViewItem* parent, giFTSearchItem* _it) : QListViewItem(parent) {
	   item = _it;
   KIconLoader *loader = KGlobal::iconLoader();
   QString icon;
   switch(item->availability().toULong()){
   case 0:
          icon = "connect_no";
          break;
   default:
          icon = "connect_established";
          break;
   }
   newcg = 0;

  setPixmap(0, loader->loadIcon(icon, KIcon::Small));
}

KadeauSearchViewSource::~KadeauSearchViewSource(){
    if(newcg)
      delete newcg;
}

void KadeauSearchViewSource::paintCell(QPainter *p, const QColorGroup & cg, int column, int width, int alignment){
    int avail = item->availability().toInt();
    if(avail == 0 ){
      if(!newcg){
        newcg = new QColorGroup(cg);
        QColor textcol = newcg->text();
        textcol.setNamedColor("darkgrey");
        newcg->setColor(QColorGroup::Text, textcol);
      }
      QListViewItem::paintCell(p,*newcg,column,width,alignment);
    }else{
      QListViewItem::paintCell(p,cg,column,width,alignment);
    }
}

QString KadeauSearchViewSource::text(int col) const{
  QString line;
      int avail;
    switch(col){
    case 0:
      line = item->user();
      break;
    case 1:
      avail = item->availability().toInt();

      if( avail == 1 ){
        line = QObject::tr("%1 slot");
      }else{
        line = QObject::tr("%2 slots");
      }
      line = line.arg(avail);
      break;

    case 2:
      if( item->hits() == 1 ){
        line = QObject::tr("%1 hit");
      }else{
        line = QObject::tr("%2 hits");
      }
      line = line.arg(item->hits());
      break;
    default:
      break;
    }

  return line;
}