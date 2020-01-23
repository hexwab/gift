/***************************************************************************
                          inisection.cpp  -  description
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

#include "inisection.h"
#include "qregexp.h"
#include <iostream>

IniSection::IniSection(){
}


IniSection::~IniSection(){
}

void IniSection::parseName(const QString & string){
   int start = string.find('[');
   start++;
   int end = string.find(']');
   int length = end - start;

   _name = string.mid(start, length);
}

void IniSection::parseLine(const QString & line){
   int equals = line.find('=');

   QString key = line.left(equals);
   QString value = line.mid(equals+1);
   key.replace(QRegExp(" "), "");
   value.replace(QRegExp(" "), "");

   keys.append(key);
   values.append(value);
}

IniSection::IniSection(QStringList strlist){
// This should be a list of lines, starting [name]
// then  key = value

   parseName(strlist[0]);

   for(unsigned int i = 1 ; i < strlist.count() ; i++){
      parseLine(strlist[i]);
   }
}


QString IniSection::toString(){
  QString string;
  string.append("#IniSection from kadeau\n");


  string.append( QString("[%1]\n").arg(_name));

   for (unsigned int i = 0; i < keys.count() ; i++){

        QString attrib("%1=%2\n");
        string.append(attrib.arg(keys[i]).arg(values[i]));
   }

  return string;
}

const QString & IniSection::value(const QString & key){
  unsigned int index = keys.findIndex(key);

  if( index < 0 || index > values.count() ){
     return QString::null;
  }else{
     return values[index];
  }
}