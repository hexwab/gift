/***************************************************************************
                          kadeautransferviewitem.cpp  -  description
                             -------------------
    begin                : Thu Apr 4 2002
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

#include "kadeautransferviewitem.h"
#include "kadeauutil.h"
#include "gifttransferevent.h"
#include "gifttransferdownloadevent.h"
#include "gifttransferuploadevent.h"
#include <iostream>
#include <kglobal.h>
#include <kiconloader.h>

KadeauTransferViewItem::KadeauTransferViewItem(QListView* parent, giFTTransferEvent* _transfer):QListViewItem(parent){
   transfer = _transfer;
   giFTTransferDownloadEvent* dl;
   giFTTransferUploadEvent* ul;
   dl = dynamic_cast<giFTTransferDownloadEvent*>(transfer);
   QString icon;
   if(dl){
      _name = dl->save();
      icon = "1downarrow";
   }else{
      ul = dynamic_cast<giFTTransferUploadEvent*>(transfer);
      if(ul){
        _name = KadeauUtil::extract_name(ul->href());
        icon = "1uparrow" ;
      }
   }
   KIconLoader *loader = KGlobal::iconLoader();
   setPixmap(0, loader->loadIcon(icon, KIcon::Small));
}

KadeauTransferViewItem::~KadeauTransferViewItem(){
	
}

// | Filename | Size | Transmitted | Bandwidth  | Progress

QString KadeauTransferViewItem::key ( int col, bool ascending) const
{
  	QString str;

    switch(col){
			case 0:
				str = _name;	
        break;
      case 1:
				str.sprintf("%012li", transfer->size());
			  break;
			case 2:
				str.sprintf("%012li", transfer->transmit());
				break;
			case 3:
				if (transfer->isFinished()){
      			str = "C";
    		}else{
						str.sprintf("%012li ",transfer->bandwidth());


    		}	
    		break;
			case 4:
  			 str = KadeauUtil::keyProgress(0,  // start
													transfer->transmit(),
													transfer->size(),
													transfer->size());
	       break;
	    default:
				break;
		}
		
		return str;	
}


QString KadeauTransferViewItem::text(int col) const{
  QString str;
  
  switch(col){
  case 0: 
   	str = _name;
		break;
  case 1:
    str = KadeauUtil::convert_size(transfer->size());
    break;
  case 2:
    str = KadeauUtil::convert_size(transfer->transmit());
    break;
  case 3:
    if (transfer->isFinished() && transfer->isCancelled()){
       str = "Cancelled";
    }else if (transfer->isFinished() && !transfer->isCancelled()){
       str = "Completed";
    }else if (!transfer->isFinished() && transfer->isCancelled()){
       str = "Cancelling";
    }else{
      unsigned long bw = transfer->bandwidth();
      if (bw){
          str = KadeauUtil::convert_size(bw);
          str += "/s ";

          int secs = (transfer->size() - transfer->transmit()) / bw;
          str += KadeauUtil::convert_time(secs);
      }else{
          str = "Stalled";
      }
    }
    break;
  default:
    break;
  }
  
  return str;
}



void  KadeauTransferViewItem::paintCell(QPainter *p, const QColorGroup & cg, int column, int width, int alignment){
     giFTTransferDownloadEvent* dl = dynamic_cast<giFTTransferDownloadEvent*>(transfer);

     switch(column){
     case 3:

        if(dl){
           if(dl->isSearching()){
              KIconLoader *loader = KGlobal::iconLoader();
              setPixmap(3, loader->loadIcon("find", KIcon::Small) );
           }else{
             setPixmap(3, NULL );
           }
        }
        QListViewItem::paintCell(p,cg,column,width,alignment);
        break;
     case 4:
          // progress
          KadeauUtil::paintProgress(p,cg,width,height(),alignment,
                                    0,  // start
                                    transfer->transmit(),
                                    transfer->size(),
                                    transfer->size(),
                                    isSelected());
          break;
     default:
          QListViewItem::paintCell(p,cg,column,width,alignment);
          break;
    }
}
