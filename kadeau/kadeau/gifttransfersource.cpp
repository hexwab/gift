/***************************************************************************
                          gifttransfersource.cpp  -  description
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

#include "gifttransfersource.h"

giFTTransferSource::giFTTransferSource(unsigned int id, QString user, QString href){
  _id = id;
  _user = user;
  _href = href;
  status = "Unknown";
  start = 0;
  total = 0;
  transmit = 0;
}

giFTTransferSource::~giFTTransferSource(){

}
