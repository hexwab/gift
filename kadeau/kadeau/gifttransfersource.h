/***************************************************************************
                          gifttransfersource.h  -  description
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

#ifndef GIFTTRANSFERSOURCE_H
#define GIFTTRANSFERSOURCE_H

#include <qobject.h>

/**
  *@author Robert Wittams
  */

class giFTTransferSource : public QObject  {
  unsigned int _id;
  QString _user;
  QString _href;
public:
  unsigned long start, total, transmit;
  QString status;
  const unsigned int& id(){return _id;}
  const QString& user(){return _user;}
  const QString& href(){return _href;}
  giFTTransferSource(unsigned int id, QString user, QString href);
  ~giFTTransferSource();
};

#endif
