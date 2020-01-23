/***************************************************************************
                          kadeau.cpp  -  description
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

#include "kadeau.h"
#include "qtabwidget.h"
#include "gifttransferevent.h"
#include "kadeautransfertab.h"
#include "kadeausearchtab.h"
#include "kadeauinfotab.h"
#include "inifile.h"
#include <qtabwidget.h>
#include <qstatusbar.h>
#include <qapplication.h>
#include <qvbox.h>
#include <qpopupmenu.h>
#include <kjanuswidget.h>
#include <kglobal.h>
#include <kiconloader.h>

#include <stdlib.h>
#include <iostream>

#define ID_STATUS_MSG 1

Kadeau::Kadeau(QWidget *parent, const char *name) : KMainWindow(parent, name)
{

  gcn = new giFTConnection();

  connect(gcn, SIGNAL(disconnected()),
           this, SLOT(slotDisconnected()));
  connect(gcn, SIGNAL(socketError(int)),
           this, SLOT(slotSocketError(int)));
  connect(gcn, SIGNAL(connected()),
           this, SLOT(slotConnected()));


  QString user(getenv("USER"));
  QString home(getenv("HOME"));
  QString filename(home);
  filename.append("/.giFT/ui/ui.conf");
  IniFile* uiconf = new IniFile(filename);

  IniSection* daemon = uiconf->getSection("daemon");
  QString portstr = daemon->value("port");
  int port = portstr.toInt();
  QString host = daemon->value("host");


  gcn->setParams(user,host,port);
  gcn->connectToServer();

  initStatusBar();
  initActions();
  initCentre();

  //resize(600, 400);
}

void Kadeau::initStatusBar(){
   statusBar()->message("Ready.");
}

void Kadeau::initCentre(){


  KIconLoader *loader = KGlobal::iconLoader();
  tabw = new KJanusWidget(this, "tabw", KJanusWidget::IconList);
  setCentralWidget(tabw);


  infobox = tabw->addVBoxPage(QString("Info"), NULL,
                              loader->loadIcon("1rightarrow", KIcon::Small, 32) );
  info = new KadeauInfoTab(infobox, "KadeauInfoTab", gcn);

  connect(info, SIGNAL(statusMessage(const QString&, KadeauTab*)),
          this, SLOT(slotStatusMsg(const QString&, KadeauTab*)) );

  searchbox = tabw->addVBoxPage(QString("Search"), NULL,
                              loader->loadIcon("find", KIcon::Small, 32));
  search = new KadeauSearchTab(searchbox, "KadeauSearchTab", gcn);	
  connect(search, SIGNAL(statusMessage(const QString&, KadeauTab*)),
          this, SLOT(slotStatusMsg(const QString&, KadeauTab*)) );

  transbox = tabw->addVBoxPage(QString("Transfers"), NULL,
                              loader->loadIcon("network", KIcon::Small, 32) );
  trans = new KadeauTransferTab(transbox, "KadeauTransferTab", gcn);	
  connect(trans, SIGNAL(statusMessage(const QString&, KadeauTab*)),
          this, SLOT(slotStatusMsg(const QString&, KadeauTab*)) );

  connect(trans, SIGNAL(browseUser(const QString&)),
          this, SLOT(browseUser(const QString&)) );
}

void Kadeau::switchToTransfer(){
  tabw->showPage(tabw->pageIndex(trans));
}

void Kadeau::browseUser(const QString& user){
  tabw->showPage(tabw->pageIndex(searchbox));
  search->browseUser(user);
}


void Kadeau::initActions(){
    QMenuBar* menubar = menuBar();
    QPopupMenu *filemenu = new QPopupMenu(this);
    menubar->insertItem("&File", filemenu);
    filemenu->insertItem("&Quit", qApp, SLOT(quit()));

    QPopupMenu *shares = new QPopupMenu(this);
    shares->insertItem("&Hide", this, SLOT(sharesHide()) );
    shares->insertItem("&Show", this, SLOT(sharesShow()) );
    shares->insertSeparator();
    shares->insertItem("S&ync", this, SLOT(sharesSync()) );
    menubar->insertItem("&Shares", shares);
}

Kadeau::~Kadeau()
{
}

void Kadeau::slotStatusMsg(const QString & text, KadeauTab* tab)
{
  statusBar()->clear();
  statusBar()->message(text);
}

void Kadeau::needReconnect(){
   // Disable and clear search and transfer tabs.
   searchbox->setEnabled(FALSE);
   transbox->setEnabled(FALSE);

   // Activate info tab.
   tabw->showPage(tabw->pageIndex(info));
}

void Kadeau::slotDisconnected(){
   slotStatusMsg("Disconnected from giFT daemon... ", 0);
   needReconnect();
}

void Kadeau::slotConnected(){
  searchbox->setEnabled(TRUE);
  transbox->setEnabled(TRUE);
}

void Kadeau::slotSocketError(int error){
  QString msg;

  switch(error){
     case QSocket::ErrConnectionRefused:
          msg = "Connection refused" ;
          break;
     case QSocket::ErrHostNotFound:
          msg = "Host not found" ;
          break;
     case QSocket::ErrSocketRead:
          msg = "Socket read error" ;
          break;
     }
    slotStatusMsg(msg,0);
    needReconnect();
}


void Kadeau::sharesHide(){
   sharesAction(giFTShareRequest::Hide);
}


void Kadeau::sharesShow(){
   sharesAction(giFTShareRequest::Show);
}


void Kadeau::sharesSync(){
   sharesAction(giFTShareRequest::Sync);
}


void Kadeau::sharesAction(giFTShareRequest::Action action){
   giFTRequest* req = new giFTShareRequest(action);
   gcn->addRequest(req);
}
