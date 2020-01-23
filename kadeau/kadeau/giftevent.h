/***************************************************************************
                          giftevent.h  -  description
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

#ifndef GIFTEVENT_H
#define GIFTEVENT_H

#include <qobject.h>

/**
  *@author Robert Wittams
  */

class XmlTag;
class giFTRequest;
class giFTEventRequest;

class giFTEvent: public QObject {
Q_OBJECT
protected:
  unsigned long _id;
  bool _isFinished;
public:
  class Listener{
  public:
    virtual void changed(unsigned long id) = 0;
    virtual void finished(unsigned long id) = 0;
    virtual void sendRequest(giFTRequest* req) = 0;
    virtual void sendEventRequest(giFTEventRequest* req) = 0;
  };

  const unsigned long& id(){return _id;}

  bool isFinished();
  giFTEvent();
  ~giFTEvent();

  virtual void process();
  virtual void handleXml(XmlTag* xml) = 0;
  virtual void cancel();
signals:
  void changed(unsigned long id);
  void finished(unsigned long id);
  void sendRequest(giFTRequest* req);
  void sendEventRequest(giFTEventRequest* req);
};



#endif
