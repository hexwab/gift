/***************************************************************************
                          giftconnection.h  -  description
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

#ifndef GIFTCONNECTION_H
#define GIFTCONNECTION_H

#include <qobject.h>
#include <qqueue.h>
#include <qintdict.h>
#include <qlist.h>
#include <qsocket.h>

/**
  *@author Robert Wittams
  */



class giFTEvent;
class giFTTransferEvent;
class giFTSearchEvent;

class giFTRequest;
class giFTEventRequest;

class QSocket;
class XmlTag;

class giFTConnection : public QObject  {
Q_OBJECT
private:
  QSocket socket;
  QString _user;
  QString _host;
  int _port;
  bool paramsSet;
  QIntDict<giFTEvent> events; // events the daemon is currently doing or finished ones the gui still wants.
  QIntDict<giFTEvent> dontcareevents; // events that the gui no longer wants.
  QQueue<giFTEventRequest> requests;  // event requests that have been made but not yet acknowledged by the daemon.
  QQueue<giFTRequest> pending; // requests that need to be sent to the daemon.

  void examineXml(XmlTag* tag);
  void cleanup();
public:
  giFTConnection();
  ~giFTConnection();

  const QString & user(){ return _user;}
  const QString & host(){ return _host;}
  const int & port(){return _port;}

  void dontCare(giFTEvent* event);
  void setParams(QString user, QString host, int port);
  bool connectToServer();
  void disconnect();
  bool isConnected();
signals:
  void newTransfer(giFTTransferEvent* transfer);
  void disconnected();
  void connected();
  void socketError(int error);


public slots:
  void addRequest(giFTRequest* req);
  void addEventRequest(giFTEventRequest* req);
  void sockWrite();
  void sockRead();
  void sockClosed();
  void sockError(int);
  void eventFinished(unsigned long);
};

#endif
