/***************************************************************************
                          xmltag.h  -  description
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

#ifndef XMLTAG_H
#define XMLTAG_H


/**
  *@author Robert Wittams
  */

#include <qstring.h>
#include <qvaluelist.h>

class XmlTag {
private:
  const char* text;

  QString _name;
  QValueList<QString> keys;
  QValueList<QString> values;

  bool eatwhitespace();
  bool getname();
  bool attribs();
  bool attrib();
public:
  //QString name;
  //QValueList<QString> keys;
  //QValueList<QString> values;
  XmlTag();
  XmlTag(QString string);
  ~XmlTag();
  const QString & value(const QString & key);
  void setValue(const QString & key, const QString & value);
  const QString & name();
  void setName(const QString & name);
  QString toString();
  XmlTag & append(const QString & key, const QString & value);
};

#endif
