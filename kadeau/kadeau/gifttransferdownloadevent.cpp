/***************************************************************************
                          gifttransferdownloadevent.cpp  -  description
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

#include "gifttransferdownloadevent.h"
#include "gifttransfereventsourcerequest.h"
#include "gifttransfersource.h"
#include "giftsearchitem.h"
#include "giftsearchrequest.h"
#include "giftsearchevent.h"
#include "xmltag.h"
#include <iostream>



giFTTransferDownloadEvent::giFTTransferDownloadEvent(unsigned long idno, int sz,
                        QString hsh, QString fname, QList<giFTSearchItem>* _sources):giFTTransferEvent(){
   _id = idno;
   _size = sz;
   _hash = hsh;
   _save = fname;
   potentials = _sources;
   _transmit = 0;
   _bandwidth = 0;
   _stalledFor = 0;
   _isFinished = FALSE;
   _isCancelled = FALSE;
   sev = NULL;
   ser = NULL;
}

giFTTransferDownloadEvent::giFTTransferDownloadEvent(unsigned long idno, XmlTag *tag){
  if(tag->name().compare("transfer") != 0 ){
      cerr<< "not transfer in download: " << tag->toString() << endl;
      return;
  }
  if(tag->value("action").compare("download") != 0){
     cerr<< "not download: " << tag->toString() << endl;
     return;
  }
  _id = idno;
  _save = tag->value("save");
  _hash = tag->value("hash");
  _size = tag->value("size").toULong();

  _transmit = 0;
  _bandwidth = 0;
  _stalledFor = 0;
  _isFinished = FALSE;
  _isCancelled = FALSE;

  sev = NULL;
  ser = NULL;
}

void giFTTransferDownloadEvent::process(){
    requestSources();
}

giFTTransferDownloadEvent::~giFTTransferDownloadEvent(){
}


void giFTTransferDownloadEvent::handleXml(XmlTag *tag){
   QString href = tag->value("addsource");
   if(! href.isNull() ){
      // addSource	
      addSource(_id,href, tag->value("user"));
   }else{
          href = tag->value("delsource");
          if (!href.isNull() ){
              //delSource
              delSource(_id,href, tag->value("user"));
          }else{
              //something else

              //cerr<<"Download passing on to giFTTransferEvent::handleXml"<< tag->toString()<<endl;
              giFTTransferEvent::handleXml(tag);
          }
   }

  // Do a search on ones which have been stalled a multiple of the idle time.

  if(stalledFor() && (stalledFor() % idleTime == 0)){
     sourceSearch();
  }
}




bool giFTTransferDownloadEvent::isSearching(){
  return (ser || sev);
}

QDictIterator<giFTTransferSource>* giFTTransferDownloadEvent::sourceIterator(){
  return new QDictIterator<giFTTransferSource>(sources);
}

void giFTTransferDownloadEvent::sourceSearch(){
  // We should not call this if it is already searching.
  if(isSearching()){
    return;
  }
    // Make a search request.
  ser = new giFTSearchRequest(_hash, "", "","hash");
    // Make sure we get the event.

  connect(ser, SIGNAL(madeEvent(giFTEvent*)),
          this, SLOT(searchEvent(giFTEvent*)) );
  emit changed(_id);
  emit sendEventRequest(ser);
}


void giFTTransferDownloadEvent::searchEvent(giFTEvent* event){
    sev = dynamic_cast<giFTSearchEvent*>(event);
    if(sev){
       ser = NULL;
       connect(sev, SIGNAL(newSource(const QString &, giFTSearchItem*)),
               this, SLOT(newSourceFound(const QString &, giFTSearchItem*)));
       connect(sev, SIGNAL(finished(unsigned long)),
               this, SLOT(sourceSearchFinished(unsigned long)));

    }
}

void giFTTransferDownloadEvent::sourceSearchFinished(unsigned long _id){
    if (sev){
    //FIXME: Tell connection we don't want this anymore.
      sev = NULL;
      emit changed(_id);
    }
}

void giFTTransferDownloadEvent::newSourceFound(const QString &s, giFTSearchItem* item){
    // check size & hash.

    if(item->size() == _size && item->hash() == _hash){
      //need to check if we had this already
      bool unique = TRUE;
      QDictIterator<giFTTransferSource> it(sources);
      for(;it.current() && unique ;++it){
           giFTTransferSource* source = it.current();
           if(source->href().compare(item->href()) == 0){
              unique = FALSE;
           }
      }
      if(unique){
        requestSource(item);

      }
    }else{
      cerr<< "Wrong search result in source search: {"<< _hash<<','<<_size<<"} != {"<<item->hash()<<','<<item->size()<<"}"<<endl;
    }
}

void giFTTransferDownloadEvent::requestSource(giFTSearchItem* item){
  giFTRequest* request = new giFTTransferEventSourceRequest(_id, item);
  emit sendRequest(request);
  addSource(_id,item->href(), item->user());
}

void giFTTransferDownloadEvent::requestSources(){
  if(potentials){
    QListIterator<giFTSearchItem> it(*potentials);
    for(; it.current(); ++it){
      requestSource(it.current());
    }
  }
}

void giFTTransferDownloadEvent::addSource(unsigned int id, QString href, QString user){
  if(_id == id){
    giFTTransferSource* source = new giFTTransferSource(_id,user,href);
    sources.insert(href,source);
    emit sourceAdded(source);
  }
}

void giFTTransferDownloadEvent::delSource(unsigned int id, QString href, QString user){
      giFTTransferSource* source= sources.take(href);
      if (source){
          emit sourceDeleted(source);
          delete source;
      }
}


void giFTTransferDownloadEvent::sourceSearchCancel(unsigned long id){
     if (sev){

     }
}

void giFTTransferDownloadEvent::handleXmlChunk(XmlTag* tag){


    QString href = tag->value("source");

    if(href){
       giFTTransferSource* source = sources.find(href);
       source->status = tag->value("status");
       if(!source->status)
           source->status = "Unknown";
       source->start =  tag->value("start").toULong();
       source->total =  tag->value("total").toULong();
       source->transmit = tag->value("transmit").toULong();

    }
}
