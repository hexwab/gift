/***************************************************************************
                          kadeautab.h  -  description
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

#ifndef KADEAUTAB_H
#define KADEAUTAB_H

#include <qwidget.h>

/**
  *@author Robert Wittams
  */
class giFTConnection;

class KadeauTab : public QWidget  {
   Q_OBJECT
protected:
  QString _status;
  giFTConnection* _gcn;
  void setStatus(QString status);
public: 
  KadeauTab(QWidget *parent=0, const char *name=0, giFTConnection* gcn = 0);
  QString& status(){return _status;}
  ~KadeauTab();
signals:
  void statusMessage(const QString& string, KadeauTab* tab);
  void browseUser(const QString& user);
};

#endif
