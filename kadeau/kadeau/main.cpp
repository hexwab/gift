/***************************************************************************
                          main.cpp  -  description
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
#include <kcmdlineargs.h>
#include <kaboutdata.h>
#include <kapplication.h>

static char* description = "A gift client for QT.";

int main(int argc, char *argv[])
{

  KAboutData aboutData( "kadeau", "Kadeau",
    VERSION, description, KAboutData::License_GPL,
    "(c) 2002, Robert Wittams", 0, 0, "robert@wittams.com");
  aboutData.addAuthor("Robert Wittams",0, "robert@wittams.com");

  KCmdLineArgs::init(argc,argv, &aboutData);
  KApplication a;
  Kadeau *kadeau = new Kadeau();
  a.setMainWidget(kadeau);
  kadeau->show();  

  return a.exec();
}
