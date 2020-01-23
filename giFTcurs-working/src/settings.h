/* giFTcurs - curses interface to giFT
 * Copyright (C) 2001-2002 Göran Weinholt <weinholt@linux.nu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307,  USA.
 *
 * $Id: settings.h,v 1.3 2002/10/12 03:17:32 chnix Exp $
 */
#ifndef _SETTINGS_H
#define _SETTINGS_H

#include <stdio.h>

/* Simply open a file and handle errors. */
FILE *open_config(char *file, char *mode);

/* Get data from one of giFT's configuration files. */
char *read_gift_config(char *file, char *section, char *option);

/* Load/save configuration */
void load_configuration(void);
int save_configuration(void);

/* Get/set configuration in memory */
char *get_config(char *section, char *name, char *standard);
void set_config(char *section, char *name, char *value, int save);
void set_config_int(char *section, char *name, int value, int save);

/* free all resources */
void config_cleanup(void);
#endif
