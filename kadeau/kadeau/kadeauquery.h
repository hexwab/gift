/***************************************************************************
                          kadeauquery.h  -  description
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
#include <qstring.h>
#include <qpixmap.h>


struct KadeauQueryType{
    QString display;
    QString typestr;
    QString realmstr;
    QPixmap icon;
    KadeauQueryType(const QString &d,const QString& t, const QString& r, QPixmap i){
        display = d;
        typestr = t;
        realmstr = r;
        icon = i;
    }
};

struct KadeauQuerySpec{
    KadeauQueryType* type;
    QString query;
    KadeauQuerySpec(KadeauQueryType* t, const QString& q ){
         type = t;
         query = q;
    }
};