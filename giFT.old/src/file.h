/*
 * file.h
 *
 * Copyright (C) 2001-2002 giFT project (gift.sourceforge.net)
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

#ifndef __FILE_H
#define __FILE_H

/*****************************************************************************/

int   file_exists    (char *file, size_t *size, time_t *mtime);
char *file_basename  (char *file);
char *file_read_line (FILE *f, char **outbuf);
FILE *file_temp      (char **out, char *module);

/*****************************************************************************/

#endif /* __FILE_H */

