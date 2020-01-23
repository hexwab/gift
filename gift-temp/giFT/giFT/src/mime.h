/*
 * $Id: mime.h,v 1.7 2003/02/09 22:54:34 jasta Exp $
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

#ifndef __MIME_H
#define __MIME_H

/*****************************************************************************/

char *mime_type (char *file);
char *mime_type_lookup (char *mime);

/*****************************************************************************/

#endif /* __MIME_H */
