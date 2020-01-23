/***************************************************************************
                          inifile.cpp  -  description
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


#include <qfile.h>
#include "inifile.h"
#include <iostream>

IniFile::IniFile(QString filename){
    QFile file(filename);

    file.open(IO_ReadOnly);

    QString line;

    QStringList strlist;
    //prefs
    while(!file.atEnd()){
        file.readLine(line, 1024);
        line = line.stripWhiteSpace();
        if(line[0] == '#'){
           // comment

           // cerr << "Comment:" << line << endl;
        }else if( line.isEmpty() ){
           // empty line
           if(strlist.count() > 0){
              sections.append(new IniSection(strlist));
              strlist.clear();
           }
        }else{
          strlist.append(line);
        }
    }

    if(strlist.count() > 0){
       sections.append(new IniSection(strlist));
    }
}


IniFile::~IniFile(){
}

IniSection* IniFile::getSection(const QString & name){
    QListIterator<IniSection> it(sections);
    IniSection* section;

    for(; it.current() ; ++it){
         section = it.current();
         if( section->name().compare(name) == 0 ){
            return section;
         }
    }
    return NULL;
}
