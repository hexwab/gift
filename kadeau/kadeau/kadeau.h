/***************************************************************************
                          kadeau.h  -  description
                             -------------------
    begin                : Mon Apr  1 14:22:34 BST 2002
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

#ifndef KADEAU_H
#define KADEAU_H

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <qwidget.h>
#include <kmainwindow.h>
#include <kmenubar.h>

#include "giftconnection.h"
#include "giftsharerequest.h"


class giFTConnection;
class QVBox;
class KJanusWidget;
class KConfig;
class KadeauTab;
class KadeauTransferTab;
class KadeauSearchTab;
class KadeauInfoTab;



/** Kadeau is the main widget of the project */
class Kadeau : public KMainWindow
{
  Q_OBJECT
  private:
    giFTConnection* gcn;
    KJanusWidget* tabw;

    QVBox *infobox, *searchbox, *transbox;

    // put in dict?
    KadeauTransferTab* trans;
    KadeauSearchTab* search;
    KadeauInfoTab* info;

    void initCentre();
    void initStatusBar();
    void initActions();

    void saveOptions();
    void readOptions();

    void sharesAction(giFTShareRequest::Action action);



  //  KAction* fileQuit;
  //  KToggleAction* viewToolBar;
  //  KToggleAction* viewStatusBar;
  public:
    /** constructor */
    Kadeau(QWidget* parent=0, const char *name=0);
    /** destructor */
    ~Kadeau();
  public slots:
    void needReconnect();
    void browseUser(const QString& user);
    void slotSocketError(int error);
    void slotDisconnected();
    void slotConnected();
    void switchToTransfer();
    void slotStatusMsg(const QString& text, KadeauTab* tab);
  /** No descriptions */
  void sharesSync();
  /** No descriptions */
  void sharesShow();
  /** No descriptions */
  void sharesHide();
};

#endif
