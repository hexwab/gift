/***************************************************************************
                          gifttransferevent.cpp  -  description
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

#include "giftcancelrequest.h"
#include "gifttransferevent.h"
#include "xmltag.h"
#include <iostream>

giFTTransferEvent::giFTTransferEvent(){
    bandwidthSet = FALSE;
    transmitSet = FALSE;
}

giFTTransferEvent::~giFTTransferEvent(){

}

void giFTTransferEvent::handleXmlTransfer(XmlTag* tag){
    QString transString = tag->value("transmit");
    if (! transString.isNull() ){
        unsigned long newTransmit = transString.toULong();
        // it is a transmit.

        // work out bandwidth.
        unsigned long curr = newTransmit - _transmit; 	

        _transmit = newTransmit; 	    	

        if (!bandwidthSet){
            if(transmitSet){
              _bandwidth = curr;
              bandwidthSet = TRUE;
            }else{
             transmitSet = TRUE;
            }
        }else{
           // This is an Estimated Weighted Moving Average.
           // As seen in giFTcurs.
           _bandwidth = ( (curr * currWeight) + (_bandwidth * lastWeight) )/totalWeight ;
        }

        if(_bandwidth == 0)
           _stalledFor++;
        else
           _stalledFor = 0;
        emit changed(_id);
    }else{
      // finish
      if(!_isCancelled){
      // work out if this cancelled at the other end
         if( _transmit < _size)
             _isCancelled = TRUE;
      }

      _isFinished = TRUE;
      emit changed(_id);	
      emit finished(_id);
    }
}

void giFTTransferEvent::handleXmlChunk(XmlTag* tag){
   // Do nothing here for now...
}

void giFTTransferEvent::handleXml(XmlTag* tag){
  // check it is appropriate.

  if(tag->name().compare("transfer") == 0 ){
     handleXmlTransfer(tag);
  }else if(tag->name().compare("chunk") == 0 ){
     handleXmlChunk(tag);
  }else{
     cerr<< "unknown tag given to transfer" << tag->toString() <<endl;
  }
}


void giFTTransferEvent::cancel(){
  giFTRequest* request = new giFTCancelRequest("transfer", _id);
  _isCancelled = TRUE;
  emit sendRequest(request);
  emit changed(_id);
}


	


