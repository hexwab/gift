/***************************************************************************
                          kadeautab.cpp  -  description
                             -------------------
    begin                : Wed Apr 10 2002
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

#include "kadeautab.h"
#include "qlayout.h"

KadeauTab::KadeauTab(QWidget *parent, const char *name, giFTConnection* gcn ) : QWidget(parent,name) {
  setStatus("Ready.");
  QBoxLayout* l = new QVBoxLayout(this);
  l->setAutoAdd(TRUE);
  _gcn = gcn;
}

KadeauTab::~KadeauTab(){


}

void KadeauTab::setStatus(QString status){
   _status = status;
   emit statusMessage(_status, this);
}