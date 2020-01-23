/***************************************************************************
                          kadeausearchitemview.cpp  -  description
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

#include "kadeausearchviewitem.h"
#include "kadeauutil.h"
#include "giftsearchitem.h"
#include <qurl.h>
#include <iostream>
#include <kglobal.h>
#include <kiconloader.h>


KadeauSearchViewItem::KadeauSearchViewItem(QListView* parent, QString the_key): QListViewItem(parent){
  best_name = QString("<none>");
  size = 0;
  no_name = TRUE;
  _key = the_key;
  sources = new QList<giFTSearchItem>();
  KIconLoader *loader = KGlobal::iconLoader();
  setPixmap(0, loader->loadIcon("mime_empty", KIcon::Small));
  newcg = 0;
}

KadeauSearchViewItem::~KadeauSearchViewItem(){
  if(newcg)
     delete newcg;
}

QString KadeauSearchViewItem::key ( int col, bool ascending) const
{
  	QString str;

    switch(col){
      case 0:
        str = best_name;	
        break;
      case 1:
        str.sprintf("%012li", size);
        break;
      case 2:
        str.sprintf("%04i%04i", availability(), sources->count());
        break;
      default:
        break;
    }

    return str;	
}

QString & KadeauSearchViewItem::getKey(){
    return _key;
}

int KadeauSearchViewItem::availability () const {
    QListIterator<giFTSearchItem> it(*sources);

    int avail = 0;

    for(; it.current(); ++it){
        giFTSearchItem* si = it.current();

        avail += si->availability().toInt() ? 1 : 0;
    }
    return avail;
}

QString KadeauSearchViewItem::text ( int col) const{
    QString line;

    switch(col){
      case 0:
        line = best_name;
        break;
      case 1:
        // turn it into human readable
        line = KadeauUtil::convert_size(size);
        break;
      case 2:
        line = QString::number(availability() );
        line += "/";
        line += QString::number(sources->count());

        break;
     default:
        break;
    }
    return line;
}

void KadeauSearchViewItem::paintCell(QPainter *p, const QColorGroup & cg, int column, int width, int alignment){


    if(availability() == 0 ){
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


static QString better_name( QString a, QString b){

    // we could do some clever merging here

    if ( a.length() < b.length() ){
        return b;
    }else{
        return a;
    }
}

void  KadeauSearchViewItem::addSource(giFTSearchItem* item){
   sources->append(item);
   if(no_name){
      best_name = KadeauUtil::extract_name( item->href() );
      no_name = FALSE;
   }else{
      QString new_name = KadeauUtil::extract_name( item->href() );
      // if the next name coming along is better, use it. 		
      best_name = better_name(new_name, best_name);
   }

   if (size == 0){
      size = item->size();
   }

   if (hash.isNull()){
      hash = item->hash();
   }
}