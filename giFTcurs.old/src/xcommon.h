/* giFTcurs - curses interface to giFT
 * Copyright (C) 2001, 2002, 2003 G�ran Weinholt <weinholt@dtek.chalmers.se>
 * Copyright (C) 2003 Christian H�ggstr�m <chm@c00.info>
 *
 * This file is part of giFTcurs.
 *
 * giFTcurs is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * giFTcurs is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with giFTcurs; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,  USA.
 *
 * $Id: xcommon.h,v 1.27 2003/05/14 09:09:37 chnix Exp $
 */
#ifndef _XCOMMON_H
#define _XCOMMON_H

#include <stdarg.h>

/* open a tcp connection, return -1 on error */
int xconnect(const char *host, const char *port);

#endif
