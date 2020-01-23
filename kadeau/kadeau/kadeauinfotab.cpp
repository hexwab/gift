/***************************************************************************
                          kadeauinfotab.cpp  -  description
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

#include "kadeauinfotab.h"
#include "giftstatsrequest.h"
#include "giftconnection.h"
#include <qlabel.h>
#include <qtextview.h>
#include <qpushbutton.h>
#include <iostream>
#include <qtimer.h>
#include <qhbox.h>
#include <kadeauutil.h>

KadeauInfoTab::KadeauInfoTab(QWidget* parent, const char *name,
  giFTConnection* gcxn): KadeauTab(parent, name, gcxn){

  connect(_gcn, SIGNAL(connected()),
          this, SLOT(connected()) );
  connect(_gcn, SIGNAL(disconnected()),
          this, SLOT(disconnected()));
  connect(_gcn, SIGNAL(socketError(int)),
          this, SLOT(disconnected()));


  new QLabel("<H1>Kadeau</H1>", this);

  QHBox* box = new QHBox(this);
  connection = new QLabel("", box);
  connbutton = new QPushButton("Connecting", box);
  connbutton->setEnabled(FALSE);
  daemonbutton = new QPushButton("Start Daemon", box);
  daemonbutton->setEnabled(FALSE);
  connect(daemonbutton, SIGNAL(clicked()),
          this, SLOT(startDaemon()) );


  infobox = new QTextView(this);

  statstimer = new QTimer(this);
  connect(statstimer, SIGNAL(timeout()),
          this, SLOT(requestStats()));

  displayConnection();
}


KadeauInfoTab::~KadeauInfoTab(){

}

void KadeauInfoTab::displayConnection(){
    QString server;

    server.append(_gcn->host()).append(":").append(QString::number(_gcn->port()));

    QString conn;

    if (_gcn->isConnected())
        conn.append("Connected to ");
    else
        conn.append("Disconnected from ");

    conn.append(server);

    connection->setText(conn);
    setStatus(conn);
}

void KadeauInfoTab::connectSocket(){
   // We are going to try to connect to the socket now.
   // Disable button, make it say "Connecting"
   connbutton->setEnabled(FALSE);
   connbutton->setText("Connecting");
   daemonbutton->setEnabled(FALSE);
   _gcn->connectToServer();

}

void KadeauInfoTab::connected(){
  // We have just been connected to the socket
  // Enable button, make it say "Disconnect"
  connbutton->setEnabled(TRUE);
  connbutton->setText("Disconnect");
  connbutton->disconnect(this);
  connect(connbutton, SIGNAL(clicked()),
          this, SLOT(disconnect()) );

  daemonbutton->setEnabled(FALSE);
  // Update stats and enable stats timer.
  requestStats();
  statstimer->start(60*1000); // 1 minute.
  displayConnection();
}

void KadeauInfoTab::disconnect(){
   // We are going to disconnect from the socket.
   // Disable button, make it say "Disconnecting".
   connbutton->setEnabled(FALSE);
   connbutton->setText("Disconnecting");
   _gcn->disconnect();
}

void KadeauInfoTab::disconnected(){
   // We have just been disconnected from the socket.
   // Enable button, make it say "Connect".
   connbutton->setEnabled(TRUE);
   connbutton->setText("Connect");
   connbutton->disconnect(this);
   connect(connbutton, SIGNAL(clicked()),
          this, SLOT(connectSocket()) );
   daemonbutton->setEnabled(TRUE);
   infobox->setText("");
   displayConnection();

   // Disable stats timer.

   statstimer->stop();
}

void KadeauInfoTab::requestStats(){

  giFTStatsRequest* req = new giFTStatsRequest();
  connect(req, SIGNAL(madeEvent(giFTEvent*)),
          this, SLOT(statsEvent(giFTEvent*)) );
  _gcn->addEventRequest(req);

}

void KadeauInfoTab::statsEvent(giFTEvent* event){
     giFTStatsEvent* newstats = dynamic_cast<giFTStatsEvent*>(event);

     if(newstats){
        stats = newstats;

        connect(stats, SIGNAL(newProtocol(giFTStatsEvent::ProtocolInfo*)),
                this, SLOT(statsProtocol(giFTStatsEvent::ProtocolInfo*)) );
        infobox->setText("");
     }
}


void KadeauInfoTab::statsProtocol(giFTStatsEvent::ProtocolInfo* prot){
  QString protocol;


  protocol.append("<p><h3><b>").append(prot->protocol).append(":</b></h3></p>");
  protocol.append("<ul>");
  if(prot->protocol.compare("local") != 0){
     protocol.append("<li><i>Status: </i><b>").append(prot->status).append("</b></li>");
     protocol.append("<li><i>Users: </i><b>").append(QString::number(prot->users)).append("</b></li>");
  }
  protocol.append("<li><i>Files Shared: </i><b>").append(QString::number(prot->files)).append("</b></li>");
  protocol.append("<li><i>Total Size Shared: </i><b>").append(QString::number(prot->size)).append("G</b></li>");
  protocol.append("</ul>");
  infobox->setText(infobox->text().append(protocol));
}


void KadeauInfoTab::startDaemon(){
    KadeauUtil::start_daemon();
}
