/* giFTcurs - curses interface to giFT
 * Copyright (C) 2001, 2002, 2003 Göran Weinholt <weinholt@dtek.chalmers.se>
 * Copyright (C) 2003 Christian Häggström <chm@c00.info>
 *
 * This file is part of giFTcurs.
 *
 * giFTcurs is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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
 * $Id: get.h,v 1.69 2003/07/22 23:58:37 weinholt Exp $
 */
#ifndef _GET_H
#define _GET_H

#include "search.h"
#include "transfer.h"

extern transfer_tree downloads;

/* start download the hit, calling a tranfer_screen handler
 * when progress updates */
int download_hit(hit *, subhit *single);

/* search again for more sources to download */
int download_search(transfer *);

/* remove a download from list (giFT continues to d/l) */
void download_forget(transfer *);

transfer *find_download(char *hash, unsigned int size);

#endif
