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
 * $Id: screen.h,v 1.74 2003/06/27 11:20:14 weinholt Exp $
 */
#ifndef _SCREEN_H
#define _SCREEN_H

/* If you change/extend this list, reflect on the strings in screen.c */
enum colors {
	COLOR_STANDARD = 1,			/* "\va" */
	COLOR_HEADER,				/* "\vb" */
	COLOR_SEARCH_BOX,			/* "\vc" */
	COLOR_RESULT_BOX,			/* "\vd" */
	COLOR_STAT_BOX,				/* "\ve" */
	COLOR_STAT_DATA,			/* "\vf" */
	COLOR_STAT_DATA_BAD,		/* "\vg" */
	COLOR_INFO_BOX,				/* "\vh" */
	COLOR_DOWNLOAD_BOX,			/* "\vi" */
	COLOR_UPLOAD_BOX,			/* "\vj" */
	COLOR_HELP_BOX,				/* "\vk" */
	COLOR_CONSOLE_BOX,			/* "\vl" */
	COLOR_HIT_GOOD,				/* "\vn" */
	COLOR_HIT_BAD,				/* "\vn" */
	COLOR_PROGRESS,				/* "\vo" */
	COLOR_TOTAL_PROGRESS,		/* "\vp" */
	COLOR_DIAMOND,
	COLOR_SELECTION,
	COLOR_BUTTON_BAR,
	COLOR_BUTTON_BAR_SEL,
	COLOR_BUTTON_BAR_ALERT,
	ITEMS_NUM = COLOR_BUTTON_BAR_ALERT
};

extern int max_x, max_y;

extern int INVERT_SEL;

void screen_init(void);
void set_default_colors(void);
int save_color_config(void);
void remap_linechars(void);
int get_item_number(char *color);

/* Used for "exporting" the items[] list. */
char *item_name(int);

/* OSF1 curses thinks alot ncurses does is non-standard. */
#ifdef USE_SYSV_CURSES
#define _XOPEN_SOURCE_EXTENDED
#endif

#if defined(USE_NCURSES) && !defined(RENAMED_NCURSES)
# include <ncurses.h>
#else
# include <curses.h>
#endif

#if (NCURSES_MOUSE_VERSION == 1) && !defined(DISABLE_MOUSE)
# define MOUSE
#endif

#ifdef MOUSE
# if !defined(DISABLE_INTERNAL_MOUSE) && !defined(NCURSES_970530)
#  define MOUSE_INTERNAL
# endif
extern gboolean use_mouse;
#endif

/* Stolen from mc... */
#define XCTRL(x) ((x) & 31)

#ifdef __CYGWIN__
# undef ACS_ULCORNER
# define ACS_ULCORNER	','
# undef ACS_URCORNER
# define ACS_URCORNER	'.'
# undef ACS_LLCORNER
# define ACS_LLCORNER	'`'
# undef ACS_LRCORNER
# define ACS_LRCORNER	'\''
# undef ACS_HLINE
# define ACS_HLINE		'-'
# undef ACS_VLINE
# define ACS_VLINE		'|'
# undef ACS_RTEE
# define ACS_RTEE		'('
# undef ACS_LTEE
# define ACS_LTEE		')'
# undef ACS_DIAMOND
# define ACS_DIAMOND	'*'
#endif

/* translate default color from and to 255 <-> 8 */
#ifdef HAVE_USE_DEFAULT_COLORS
int my_init_pair(short pair, short fg, short bg);
int my_pair_content(short pair, short *fgp, short *bgp);

# define init_pair my_init_pair
# define pair_content my_pair_content
#endif

#define COLOR(c,f)				(COLOR_PAIR(c) | (active_field == f ? A_BOLD : 0))
#define COLOR_BUTTON(c,f,e)		(COLOR_PAIR(c) | (e ? A_BOLD : 0) | (active_field == f ? A_REVERSE : 0))

/* english names for the basic colours */
#ifdef HAVE_USE_DEFAULT_COLORS
extern const char *colornames[9];
#else
extern const char *colornames[8];
#endif

#ifdef __linux__
gboolean shift_pressed(void);
#else
# define shift_pressed() (FALSE)
#endif

/* The type of a log hook function. */
typedef void (*LogFunc) (GLogLevelFlags, const char *);

/* Initialize the logging system. */
void log_init(void);

/* Add a logging hook. */
void log_add_hook(LogFunc func);
void log_remove_hook(LogFunc func);

#endif
