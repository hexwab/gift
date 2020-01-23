/***************************************************************************
                          gifttransferevent.h  -  description
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

#ifndef GIFTTRANSFEREVENT_H
#define GIFTTRANSFEREVENT_H

#include <giftevent.h>
#include <qlist.h>
#include <qdict.h>

/**
  *@author Robert Wittams
  */


class XmlTag;

class giFTTransferEvent : public giFTEvent  {
Q_OBJECT
private:
  // constants for bandwidth calculation.
  // similar to giFTcurs
  static const int totalWeight= 256;
  static const int currWeight = 32;
  static const int lastWeight = totalWeight - currWeight;

  bool bandwidthSet;
  bool transmitSet;
protected:
  virtual void handleXmlTransfer(XmlTag* tag);
  virtual void handleXmlChunk(XmlTag* tag);
  unsigned long _size;
  unsigned long _transmit;
  unsigned long _bandwidth;
  unsigned long _stalledFor;
  bool _isCancelled;
public:
  giFTTransferEvent();
  ~giFTTransferEvent();

  const unsigned long& size(){return _size;}
  const unsigned long& transmit(){return _transmit;}
  const unsigned long& bandwidth(){return _bandwidth;}
  const unsigned long& stalledFor(){return _stalledFor;}
  const bool& isCancelled(){return _isCancelled;}

  virtual void handleXml(XmlTag* xml);
  void cancel();
};

#endif
