/***************************************************************************
                          gifttransferdownloadevent.h  -  description
                             -------------------
    begin                : Sun Apr 7 2002
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

#ifndef GIFTTRANSFERDOWNLOADEVENT_H
#define GIFTTRANSFERDOWNLOADEVENT_H

#include <gifttransferevent.h>


/**
  *@author Robert Wittams
  */

class giFTTransferSource;
class giFTSearchItem;
class giFTSearchRequest;
class giFTSearchEvent;

class giFTTransferDownloadEvent : public giFTTransferEvent{
Q_OBJECT
private:
  QString _save; // the local filename - downloads
  QString _hash;
  const static int idleTime = 60;
  QList<giFTSearchItem> *potentials;
  QDict<giFTTransferSource> sources;
  giFTSearchRequest* ser;
  giFTSearchEvent* sev;
  void requestSources();
  void requestSource(giFTSearchItem* item);
  void addSource(unsigned int id, QString href, QString user);
  void delSource(unsigned int id, QString href, QString user);

  void handleXmlChunk(XmlTag* tag);
public:
  giFTTransferDownloadEvent(unsigned long idno, int sz, QString hsh, QString fname, QList<giFTSearchItem>* sources);
  giFTTransferDownloadEvent(unsigned long id, XmlTag* tag);
  ~giFTTransferDownloadEvent();

  QDictIterator<giFTTransferSource>* sourceIterator();
  const QString& save(){return _save;}
  const QString& hash(){return _hash;}
  void process();
  void handleXml(XmlTag* xml);
  void sourceSearch();
  bool isSearching();
  /** No descriptions */
  void sourceSearchCancel(unsigned long id);
signals:
  void sourceAdded(giFTTransferSource* source);
  void sourceDeleted(giFTTransferSource*);
public slots:
  void sourceSearchFinished(unsigned long id);
  void newSourceFound(const QString &s, giFTSearchItem* item);
  void searchEvent(giFTEvent* event);
};

#endif
