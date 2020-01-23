/***************************************************************************
                          kadeautransferviewsource.cpp  -  description
                             -------------------
    begin                : Tue Apr 23 2002
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

#include "kadeautransferviewsource.h"
#include "kadeauutil.h"
#include "gifttransfersource.h"
#include <kglobal.h>
#include <kiconloader.h>
#include <iostream>

KadeauTransferViewSource::KadeauTransferViewSource(QListViewItem* parent , giFTTransferSource* source,unsigned long transfer_size):
    QListViewItem(parent){
  KIconLoader *loader = KGlobal::iconLoader();
  _source= source;
  _transfer_size = transfer_size;

  setPixmap(0, loader->loadIcon("connect_established", KIcon::Small));
}

KadeauTransferViewSource::~KadeauTransferViewSource(){
}


QString KadeauTransferViewSource::key ( int col, bool ascending) const
{
    QString str;

    switch(col){
      case 0:
        str = _source->user();
        break;
      case 1:
        str.sprintf("%012li", _source->total);
        break;
      case 2:
        str.sprintf("%012li", _source->transmit);
        break;
      case 3:
        str = _source->status;
      case 4:
         str.sprintf("%012li", _source->start);
         break;
      default:
        break;
    }

    return str;	
}


QString KadeauTransferViewSource::text(int col) const{
  QString str;
  if(_source){
  switch(col){
  case 0:
   	str = _source->user();
		break;
  case 1:
    str = KadeauUtil::convert_size(_source->total);
    break;
  case 2:
    str = KadeauUtil::convert_size(_source->transmit);
    break;
  case 3:
     if( _source->status)
         str = _source->status;
    break;
  default:
    break;
  }
  }
  return str;
}



void  KadeauTransferViewSource::paintCell(QPainter *p, const QColorGroup & cg, int column, int width, int alignment){
     switch(column){
     case 4:
          // progress
          if(_source->status.compare("Unknown") != 0){
            KadeauUtil::paintProgress(p,cg,width,height(),alignment,
                                    _source->start,  // start
                                    _source->start + _source->transmit,
                                    _source->start + _source->total,
                                    _transfer_size,
                                    isSelected());
          }else{
             QListViewItem::paintCell(p,cg,column,width,alignment);
          }
          break;
     default:
          QListViewItem::paintCell(p,cg,column,width,alignment);
          break;
    }
}