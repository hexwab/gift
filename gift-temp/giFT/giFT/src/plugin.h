/*
 * $Id: plugin.h,v 1.4 2003/04/25 10:44:38 jasta Exp $
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

#ifndef __PLUGIN_H
#define __PLUGIN_H

/*****************************************************************************/

Protocol *plugin_load (char *file, char *pname);
void plugin_unload (Protocol *p);

/*****************************************************************************/

#endif /* __PLUGIN_H */
