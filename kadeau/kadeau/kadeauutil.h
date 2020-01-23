/***************************************************************************
                          kadeauutil.h  -  description
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

#ifndef KADEAUUTIL_H
#define KADEAUUTIL_H


/**
  *@author Robert Wittams
  */
class QPainter;
class QColorGroup;
class QString;

class KadeauUtil {
public:
  static QString convert_size( unsigned long size);
  static QString convert_time( unsigned long secs);
  static QString extract_name( QString href);
  static void paintProgress(QPainter *p,const QColorGroup & cg,int width,int height,int alignment,
                            int start, int current, int stop, int max, bool selected);

  static QString keyProgress(int start, int current,int stop,int max);
  /** No descriptions */
  static void start_daemon();
};

#endif
