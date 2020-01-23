/***************************************************************************
                          giftconnection.cpp  -  description
                             -------------------
    begin                : Mon Apr 1 2002
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

#include "giftconnection.h"
#include "giftevent.h"
#include "giftrequest.h"
#include "gifttransferuploadevent.h"
#include "gifttransferdownloadevent.h"
#include "giftattachrequest.h"
#include "giftstatsrequest.h"
#include "gifttransfereventsourcerequest.h"
#include "xmltag.h"
#include <qsocketnotifier.h>


#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <stdarg.h>
#include <iostream>


#include <config.h>

giFTConnection::giFTConnection(){
   paramsSet = FALSE;
   connect(&socket, SIGNAL(readyRead()),
           this, SLOT(sockRead()) );
   connect(&socket, SIGNAL(connectionClosed()),
           this, SLOT(sockClosed()) );
   connect(&socket, SIGNAL(error(int)),
          this, SLOT(sockError(int)) );
   connect(&socket, SIGNAL(connected()),
           this, SIGNAL(connected()) ) ;
}

giFTConnection::~giFTConnection(){
  // Destroy all members.

}

bool giFTConnection::isConnected(){
  return (socket.state() == QSocket::Connection);
}

void giFTConnection::setParams(QString user, QString host, int port){
   _host = host;
   _port = port;
   _user = user;
   paramsSet = TRUE;
}

bool giFTConnection::connectToServer(){
   // Make a connection to the giFT daemon.
   if( paramsSet != TRUE)
         return FALSE;

   socket.connectToHost(_host, (Q_UINT16) _port);

   // Make an attach request.
   giFTRequest* attach = new giFTAttachRequest(_user, PACKAGE, VERSION);
   // addRequest.
   addRequest(attach);
   return TRUE;
}

void  giFTConnection::disconnect(){
  socket.close();
  emit disconnected();
  cleanup();
}

void giFTConnection::addEventRequest(giFTEventRequest* req){
  requests.enqueue(req);
  addRequest(req);
}

void giFTConnection::addRequest(giFTRequest* req){
  //Ask to send this request down the socket.

  // Stick in pending requests queue.
   pending.enqueue(req);
   // Write to the buffered socket.
   sockWrite();
}

void giFTConnection::examineXml(XmlTag* tag){
  unsigned long id;

  /* get id out of xml */

  QString idstring = tag->value("id");

  // cerr<< "got from Server:  " << tag->toString() << endl;

  if ( idstring.isNull() ){
      cerr<< "No id. Abandoning this tag" << endl;
      return;
  }else{
    id = idstring.toULong();
  }


  // look at each incoming "id".
  // dispatch the data according to this id via the events dict.
  giFTEvent* event = events[id];
  if( event != NULL){
      //cerr << " using existing event: " << id << endl;
      event->handleXml(tag);
  }else{

  // not in dict, so new id.
  // make a new event.
  // <event .. >  -> get off requests queue
  // <transfer ..>  -> make a transfer event. Occurs just after attach or new upload.
  // <other ...>   -> Shouldn't happen?



    if (tag->name().compare("event")  == 0){



      // Would be better if we did get an event for everything. 			
      // Is it a transfer?
      bool isTransfer = FALSE;

      QString action = tag->value("action");
      if (action && action.compare("transfer") == 0 ){
          isTransfer = TRUE;
      }

      giFTEventRequest* req = requests.dequeue();
      if(!req){
          cerr<<"No request on the queue!!!! :" << tag->toString() << endl;
          return;
      }
      event = req->makeEvent(tag);
      delete req;

      events.insert(id, event);

      // this seems a bit skag.
      if (isTransfer){
          giFTTransferEvent* transfer =  (giFTTransferEvent*) event;
          emit newTransfer(transfer);
      }

      // so the event tells us it is done.
       connect(event, SIGNAL(finished(unsigned long)),
              this, SLOT(eventFinished(unsigned long)) );

      // so events can make additional requests
      // and event requests.
      connect(event, SIGNAL(sendRequest(giFTRequest*)),
              this, SLOT(addRequest(giFTRequest*)) );

      connect(event, SIGNAL(sendEventRequest(giFTEventRequest*)),
              this, SLOT(addEventRequest(giFTEventRequest*)) );

      // Tell the event to do stuff.
      event->process();

   }else if( tag->name().compare("transfer") == 0 && !tag->value("size").isNull() ){

      // upload or download?

      giFTTransferEvent* transfer;
      if(tag->value("action").compare("download") == 0){
        transfer = new giFTTransferDownloadEvent(id, tag);
      }else{
        transfer = new giFTTransferUploadEvent(id, tag);
      }

      events.insert(id, transfer);
      // so the event tells us it is done.
      connect(transfer, SIGNAL(finished(unsigned long)),
              this, SLOT(eventFinished(unsigned long)) );

      // so events can make additional requests
      // not event requests.
      connect(transfer, SIGNAL(sendRequest(giFTRequest*)),
              this, SLOT(addRequest(giFTRequest*)) );
      connect(transfer, SIGNAL(sendEventRequest(giFTEventRequest*)),
              this, SLOT(addEventRequest(giFTEventRequest*)) );


      emit newTransfer( transfer);
      }else{
      cerr<< "something unexpected: " << tag->toString() << endl;

      // We sometimes get wierd
      //    <transfer id="126" transmit="0" total="0"/>

      //    <transfer id="126"/>


      // which don't seem to correspond to anything.

    }
  }
}





void giFTConnection::sockRead(){

  while(socket.canReadLine()){
        QString line = socket.readLine();
        XmlTag* tag = new XmlTag(line);
        examineXml(tag);
        // examineXml(XmlTag::parse(socket.readLine()));
  }
}


void giFTConnection::sockWrite(){
  giFTRequest* preq;

  while( !pending.isEmpty() ){
    // For each pending request,
    preq = pending.dequeue();
    // write it to the socket
    QString request = preq->xml()->toString();
    socket.writeBlock(request, request.length());
  }
}

void giFTConnection::sockClosed(){
    // Delete all events, transfers, and requests.
    emit disconnected();
    cleanup();
}

void giFTConnection::sockError(int error){
     emit socketError(error);
     cleanup();
}

void giFTConnection::cleanup(){
     // get rid of events...
     QIntDictIterator<giFTEvent> ite(events);
     for(; ite.current(); ++ite){
          delete ite.current();
     }
     events.clear();

     // don't care events
     QIntDictIterator<giFTEvent> itd(dontcareevents);
     for(; itd.current(); ++itd){
          delete itd.current();
     }
     dontcareevents.clear();

     //event requests to be acknowledged

     while(!requests.isEmpty()){
          delete requests.dequeue();
     }

     // pending events
     while(!pending.isEmpty()){
          delete pending.dequeue();
     }

    _host = "";
    _port = 0;
}

void giFTConnection::eventFinished(unsigned long i){
     // The event is finished on the server

   giFTEvent* event = events[i];

   if(event != NULL){
     events.remove(i);
     return;
   }

   event = dontcareevents[i];
   if(event != NULL){
     dontcareevents.remove(i);
     delete event;
     return;
   }
}


void giFTConnection::dontCare(giFTEvent* event){
// This is called by the gui on a connection that it doesn't care about anymore.
// We do not care about this event any more.
// if it is already finished, we can delete it.

  if(event->isFinished()){
    //This should already not be in events - it would have been connected.
    delete event;
  }else{
    dontcareevents.insert(event->id(), event);
    event->cancel();
  }
}
