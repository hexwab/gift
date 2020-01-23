/***************************************************************************
                          xmltag.cpp  -  description
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

#include "xmltag.h"
#include <ctype.h>
#include <iostream>
#include <string.h>


XmlTag::XmlTag(){
   text = NULL;
}

XmlTag::~XmlTag(){
}

const QString & XmlTag::value(const QString & key){
  unsigned int index = keys.findIndex(key);

  if( index < 0 || index > values.count() ){
     return QString::null;
  }else{
     return values[index];
  }
}

QString XmlTag::toString(){
  QString str;
  str += "<";
  str.append(_name);

  for(unsigned int i = 0; i < keys.count(); i++){
    if( values[i].compare("") != 0 ){
      str += " " ;
      str += keys[i];
      str += "=\"";
      str += values[i];
      str += '"';
    }
  }

  str += "/>\r\n";
  return str;
}



bool XmlTag::eatwhitespace(){
  while( isspace(*text) ){
        text++;
        if(*text == '\0' ){
            return FALSE;
        }
  }
  return TRUE;
}



bool XmlTag::getname(){	
    // eat up whitespace
    if(!eatwhitespace())
       return FALSE;
    // copy name
    QString nstr;
    while( isalpha(*text) ){
        nstr += *text;
        text++;
        if(*text == '\0'){
           return FALSE;
        }
    }
    setName(nstr);
    return TRUE;
}


bool XmlTag::attribs(){
    // eat up whitespace
    if(!eatwhitespace())
       return FALSE;
    // while not end of tag

    while( *text != '/' ){
        if(!attrib())
            return FALSE;
        if(!eatwhitespace())
            return FALSE;
    }
   return TRUE;
}

bool XmlTag::attrib(){
    //add attrib to tag.
    QString key;
    QString value;

    // read attrib name.

    while( isalpha(*text) ){
        key += *text;
        text++;
        if(text == NULL){
           return FALSE;
        }
    }

    if(!eatwhitespace())
        return FALSE;

    if(*text == '=' ){
               text++;
    }else{
          return FALSE;
    }

    // read attrib value.

    if( *text == '"'){
      text++;
      while( *text !='"' ){
        // This won't catch quoted "
          value += *text;
          text++;

          if(*text == '\0'){
             return FALSE;
          }
      }
      text++;
    }else{
       while(! isspace(*text) ){
          value += *text;
          text++;
          if(*text == '\0' ){
              return FALSE;
          }
       }
    }
    append(key,value);
    return TRUE;
}




XmlTag::XmlTag(QString string){
  text = string;
  if ( *text != '<')
        return;
  text++;

  if(!getname())
      return;
  attribs();
}

XmlTag & XmlTag::append(const QString & key, const QString & value){
  setValue(key, value);
  return *this;
}

void XmlTag::setValue(const QString & key, const QString & value){
   if( value ){
    keys.append(key);
    values.append(value);
   }
}

const QString & XmlTag::name(){
     return _name;
}

void XmlTag::setName(const QString & n){
      _name = n;
}