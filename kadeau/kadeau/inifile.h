/***************************************************************************
                          inifile.h  -  description
                             -------------------
    begin                : Tue Apr 23 2002
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

#ifndef INIFILE_H
#define INIFILE_H

#include <qlist.h>
#include "inisection.h"
/**
  *@author Robert Wittams
  */


class IniFile {
private:
  QList<IniSection> sections;
public: 
  IniFile(QString filename);
  IniSection* getSection(const QString& name);
  ~IniFile();
};

#endif
