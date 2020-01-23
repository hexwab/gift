/***************************************************************************
                          gifttransferuploadevent.cpp  -  description
                             -------------------
    begin                : Sun Apr 7 2002
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

#include "gifttransferuploadevent.h"
#include "xmltag.h"
#include <iostream.h>

giFTTransferUploadEvent::~giFTTransferUploadEvent(){
}


giFTTransferUploadEvent::giFTTransferUploadEvent(unsigned long idno, XmlTag *tag):giFTTransferEvent(){
  if(tag->name().compare("transfer") != 0 ){
      cerr<< "not transfer in upload:" << tag->toString() << endl;
      return;
  }

  if(tag->value("action").compare("upload") != 0){
    cerr<< "not upload" << endl;
  }

  _id = idno;
  _user = tag->value("user");
  _href = tag->value("href");
  _size = tag->value("size").toULong();

  _transmit = 0;
  _bandwidth = 0;
  _stalledFor = 0;

  _isFinished = FALSE;
  _isCancelled = FALSE;
}