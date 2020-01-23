/***************************************************************************
                          inisection.h  -  description
                             -------------------
    begin                : Wed Apr 17 2002
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

#ifndef INISECTION_H
#define INISECTION_H


/**
  *@author Robert Wittams
  */
#include <qstring.h>
#include <qstringlist.h>

class IniSection {
private:

  QString _name;
  QStringList keys;
  QStringList values;
  void parseName(const QString & string);
  void parseLine(const QString & line);

public:
  QString toString();
  IniSection();
  IniSection(QStringList str);
  ~IniSection();
  const QString & name(){return _name;}
  const QString & value(const QString & key);

};

#endif
