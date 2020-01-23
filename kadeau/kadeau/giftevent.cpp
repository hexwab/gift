/***************************************************************************
                          giftevent.cpp  -  description
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

#include "giftevent.h"

giFTEvent::giFTEvent(){
}

giFTEvent::~giFTEvent(){
}

bool giFTEvent::isFinished(){
  return _isFinished;
}

void giFTEvent::process(){}

void giFTEvent::cancel(){}