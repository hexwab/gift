/***************************************************************************
                          kadeauinfotab.h  -  description
                             -------------------
    begin                : Tue Apr 9 2002
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

#ifndef KADEAUINFOTAB_H
#define KADEAUINFOTAB_H

#include <kadeautab.h>
#include <giftstatsevent.h>
/**
  *@author Robert Wittams
  */

class giFTConnection;
class QLabel;
class QTextView;
class QTimer;
class QPushButton;

class KadeauInfoTab : public KadeauTab  {
Q_OBJECT
private:
   giFTStatsEvent* stats;    // latest stats update.
   QLabel* connection;
   QPushButton* connbutton;
   QPushButton* daemonbutton;
   QTextView* infobox;
   QTimer* statstimer;
   void displayConnection();
public:
    KadeauInfoTab(QWidget* parent, const char *name, giFTConnection* gcxn);
   ~KadeauInfoTab();
public slots:
   void connectSocket();
   void connected();
   void disconnect();
   void disconnected();
   void requestStats();
   void statsEvent(giFTEvent* event);
   void statsProtocol(giFTStatsEvent::ProtocolInfo* prot);
  /** No descriptions */
  void startDaemon();
};

#endif
