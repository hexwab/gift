/*
 * $Id: meta.h,v 1.5 2003/02/09 22:54:34 jasta Exp $
 *
 * Copyright (C) 2001-2003 giFT project (gift.sourceforge.net)
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 */

#ifndef __META_H
#define __META_H

/*****************************************************************************/

void  meta_set     (FileShare *file, char *key, char *value);
void  meta_clear   (FileShare *file);
char *meta_lookup  (FileShare *file, char *key);
void  meta_foreach (FileShare *file, DatasetForeach func, void *udata);
int   meta_run     (FileShare *file);

/*****************************************************************************/

#endif /* __META_H */
