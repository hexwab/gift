/***************************************************************************
                          giftsearchevent.cpp  -  description
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

#include "giftsearchevent.h"

#include "giftsearchitem.h"

#include "xmltag.h"

#include <iostream>


giFTSearchEvent::giFTSearchEvent(unsigned long idno){
  _id = idno;
  numKeys = 0;
  numHits = 0;
  _isFinished = FALSE;
}


giFTSearchEvent::~giFTSearchEvent(){
  // delete all those items.

}


void giFTSearchEvent::handleXml(XmlTag* tag){
     /* is it an item, or an event finished tag? */

    if(tag->name().compare("item") == 0 ){
        // item
        giFTSearchItem* item = new giFTSearchItem(tag);
        /* We have a dict of dicts. 1st level on keys, second on URIS. */

        /* Do we already have a dict for this key? */

        QDict<giFTSearchItem> *sources = items[item->key()];

        if ( sources == NULL){
            sources = new QDict<giFTSearchItem>;
            items.insert(item->key(), sources);	
            emit newKey( item->key() , item);
            numKeys++;
        }

        // do we already have that same URI?

        giFTSearchItem* found = sources->find( item->href() );

        if ( found == NULL ){
            sources->insert(item->href(), item);			
            emit newSource(item->key(), item);
            item->addHit();
        }else{
            found->addHit();
        }
        numHits++;
      }else if(tag->name().compare("search") == 0){
        // end of search , tell connection to put us off the queue.
        _isFinished = TRUE;
        emit finished(_id);
        // tell gui how many keys/hits we got in total.
        // could get num unique users or something?			
        emit searchComplete(numKeys,numHits);
   }

  delete tag;
}

