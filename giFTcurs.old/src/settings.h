/* giFTcurs - curses interface to giFT
 * Copyright (C) 2001, 2002, 2003 Göran Weinholt <weinholt@dtek.chalmers.se>
 * Copyright (C) 2003 Christian Häggström <chm@c00.info>
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
 * $Id: settings.h,v 1.7 2003/05/08 21:33:17 chnix Exp $
 */
#ifndef _SETTINGS_H
#define _SETTINGS_H

#include <stdio.h>

/* Simply open a file and handle errors. */
FILE *open_config(const char *file, const char *mode);

/* Get data from one of giFT's configuration files. */
char *read_gift_config(const char *file, const char *section, const char *option);

/* Load/save configuration */
void load_configuration(void);
int save_configuration(void);

/* Get/set configuration in memory */
const char *get_config(const char *section, const char *name, const char *standard);
void set_config(const char *section, const char *name, const char *value, int save);
void set_config_int(const char *section, const char *name, int value, int save);

/* free all resources */
void config_cleanup(void);

#endif
