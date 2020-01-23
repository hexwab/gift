/*
 * win32_support.h
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

#ifndef __WIN32_SUPPORT_H__
#define __WIN32_SUPPORT_H__

#ifdef WIN32

/*****************************************************************************/

#define snprintf _snprintf
#define vsnprintf _vsnprintf

#define DATA_DIR win32_data_dir ()

/*****************************************************************************/

char *win32_gift_dir ();
char *win32_data_dir ();
void win32_printf (const char *format, ...);
void win32_fatal (const char *format, ...);

/* void win32_init (int argc, char **argv); */
void win32_cleanup ();

/*****************************************************************************/

#endif /* WIN32 */

#endif /* __WIN32_SUPPORT_H__ */
