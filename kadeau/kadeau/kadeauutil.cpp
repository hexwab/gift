/***************************************************************************
                          kadeauutil.cpp  -  description
                             -------------------
    begin                : Sat Apr 6 2002
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

#include "kadeauutil.h"
#include <qapplication.h>
#include <qpainter.h>
#include <qpen.h>
#include <qbrush.h>
#include <qcolor.h>
#include <qpalette.h>
#include <qurl.h>
#include <qregexp.h>
#include <qstring.h>
#include <iostream>
#include <stdlib.h>

QString KadeauUtil::convert_size( unsigned long size)
{
		// This is out of kde . kdelibs/kio/kioglobal.c
    float fsize;
    QString s;
    // Giga-byte
    if ( size >= 1073741824 )
    {
        fsize = (float) size / (float) 1073741824;
        if ( fsize > 1024 ) // Tera-byte
            s = QString( "%1 TB" ).arg((fsize / (float)1024), 0, 'f', 1);
        else
            s = QString( "%1 GB" ).arg(fsize, 0,'f',1);
    }
    // Mega-byte
    else if ( size >= 1048576 )
    {
        fsize = (float) size / (float) 1048576;
        s = QString( "%1 MB" ).arg(fsize,0,'f',1);
    }
    // Kilo-byte
    else if ( size > 1024 )
    {
        fsize = (float) size / (float) 1024;
        s = QString( "%1 KB" ).arg(fsize,0,'f',1);
    }
    // Just byte
    else
    {
        fsize = (float) size;
        s = QString( "%1 B" ).arg(fsize,0,'f',0);
    }
    return s;
}

QString KadeauUtil::convert_time( unsigned long time)
{
    int days, hours, minutes, seconds;
    static const int MINUTE = (60);
    static const int HOUR = (60 * MINUTE);
    static const int DAY = (24 * HOUR);


    days = time / DAY;
    time -= days * DAY;

    hours = time / HOUR;
    time -= hours * HOUR;

    minutes = time / MINUTE;
    time -= minutes * MINUTE;

    seconds = time;

    QString str;

    if(days){
       str = qApp->translate("util", "%1d%2h").arg(days).arg(hours);
    }else if(hours){
       str = qApp->translate("util","%1h%2m").arg(hours).arg(minutes);
    }else if(minutes){
       str = qApp->translate("util","%1m%2s").arg(minutes).arg(seconds);
    }else{
       str = qApp->translate("util", "%1s").arg(seconds);
    }

    return str;
}

QString KadeauUtil::extract_name( QString href){
		// we could try to do something clever with the directory names	
		QUrl url(href);

    QString query = url.query();
    QUrl::decode(query);
    int reqstart = query.find(QRegExp("request_file="));

    QString name;


    if(reqstart != -1){
      int reqend = query.find('&', reqstart);

      reqstart += 13; // skip request_file

      int lastslash = query.findRev('/', reqend);

      reqstart = lastslash ? lastslash : reqstart;

      int reqlength = reqend ? reqend - reqstart : query.length() - reqstart;

      name = query.mid(reqstart, reqlength);
    }else{
      name = url.fileName();
    }
		// Maybe make this stuff an option?
    name.replace( QRegExp(" "), "_");
		name.replace( QRegExp("_-_"), "-");
		name.replace( QRegExp("_{2,}"), "_");
		return name;
}

QString KadeauUtil::keyProgress(int start,
													 	 int current,
													   int stop,
													   int max){

  QString key;
	float percentage =((  ((float)(current - start) /(float)(stop - start)  ))  * 100);
	
	if (percentage > 100.0)	
			percentage = 100.0;
	
	if (percentage < 0.0)
			percentage = 0.0;
	
	key.sprintf("%06.2f", percentage);
  return key;
}

void  KadeauUtil::paintProgress(QPainter *p,
                           const QColorGroup & cg,
                           int width,
                           int height,
                           int alignment,
                           int start,
                           int current,
                           int stop,
                           int max,
                           bool selected){
  // # dark
  // * mid
  // - light
  // |#######|******|------|######|
  // 0     start  current stop    max

  // so we can draw progress for chunk we are transfering, dark is the bit that doesn't matter.

  //work out coords.

  float scale = max <= width ? (float)max / (float)width : max/width ;     // amount of bytes per pixel
  int p_start   =(int) (start   / scale);
  int p_current =(int) (current / scale);
  int p_stop    =(int) (stop    / scale);
  int p_max     =(int) width;

  //clear off crap:

  p->fillRect( 0, 0, width, height, cg.brush( QColorGroup::Base ) );

  QBrush dark, mid, light;
  QColor text;

  dark =  cg.brush(QColorGroup::Dark);
  mid  =  cg.brush(QColorGroup::Mid);
  light = cg.brush(QColorGroup::Light);


  text =  cg.text();


  // 0-start
  //p->setPen(cg.dark());
  //p->setBrush(cg.dark());

  //p->fillRect(0        , 0, p_start               , height, dark);

  // start - current
  p->fillRect(p_start  , 0, p_current - p_start   , height,  mid );

  // current - stop
  p->fillRect(p_current, 0, p_stop    - p_current , height, light);

  // stop - max
  //p->fillRect(p_stop   , 0, p_max     - p_stop    , height, dark );


  if(selected){
     Qt::RasterOp op = p->rasterOp();
     p->setRasterOp(Qt::AndROP);
     QPen pen(cg.highlight(), 2);
     p->setPen(pen);
     p->setBrush(QBrush());
     p->drawRect( 1, 1, width-1, height-1 );
     p->setRasterOp(op);

/*     QColor highlight = cg.highlight();

     int r, g, b, a;
     unsigned int rgba;
     highlight.rgb(&r, &g, &b);
     a = 128;
     rgba = qRgba(r, g, b, a);
     QColor trans(rgba,rgba);

     p->fillRect( 0, 0, width, height,QBrush(trans));

*/
  }

  int percentage =(int)((  ((float)(current - start) /(float)(stop - start)  ))  * 100);

  if (percentage > 100)	
      percentage = 100;

  if (percentage < 0)
      percentage = 0; 	

  QString progress = QString::number(percentage);
  progress += "%";

  p->setPen(text);

  int starttext = p_start - 10;
  starttext = starttext > 0 ? starttext : 0;
  int endtext = p_stop + 10;
  endtext = endtext < width ? endtext : width;

  p->drawText(starttext ,0,endtext - starttext,height,Qt::AlignHCenter,progress);
}


void KadeauUtil::start_daemon(){
     system("giFT &");

}
