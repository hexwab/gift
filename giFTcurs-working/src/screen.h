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
 * $Id: screen.h,v 1.58 2002/10/22 20:26:26 weinholt Exp $
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
	COLOR_HIT_GOOD,				/* "\vl" */
	COLOR_HIT_BAD,				/* "\vm" */
	COLOR_PROGRESS,				/* "\vo" */
	COLOR_TOTAL_PROGRESS,		/* "\vp" */
	COLOR_DIAMOND,
	COLOR_SELECTION,
	COLOR_BUTTON_BAR,
	COLOR_BUTTON_BAR_SEL,
	ITEMS_NUM = COLOR_BUTTON_BAR_SEL
};

extern int max_x, max_y;

extern int INVERT_SEL;

void screen_init(void);
void screen_deinit(void);
void set_default_colors(void);
void load_color_config(void);
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
# if !defined(DISABLE_INTERNAL_MOUSE)
#  define MOUSE_INTERNAL
# endif
extern int use_mouse;
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
# define init_pair(pair, fg, bg) init_pair(pair, (fg)==8 ? -1 : (fg), (bg)==8 ? -1 : (bg))
# define pair_content(pair, fgp, bgp) \
	do { \
		if (has_colors()) { \
			pair_content(pair, fgp, bgp); \
			if (*(signed char*)(fgp) == -1) *(fgp) = 8; \
			if (*(signed char*)(bgp) == -1) *(bgp) = 8; \
		} else { \
			*(fgp) = COLOR_WHITE; *(bgp) = COLOR_BLACK; \
		} \
	} while (0)
#endif

#define COLOR(c,f)				(COLOR_PAIR(c) | (scr.active_field == f ? A_BOLD : 0))
#define COLOR_BUTTON(c,f,e)		(COLOR_PAIR(c) | (e ? A_BOLD : 0) | (scr.active_field == f ? A_REVERSE : 0))

/* english names for the basic colours */
#ifdef HAVE_USE_DEFAULT_COLORS
extern const char *colornames[9];
#else
extern const char *colornames[8];
#endif

#ifdef __linux__
int shift_pressed(void);
#else
# define shift_pressed() (0)
#endif

/* print something on stderr and status line */
void do_log(int in_status, const char *fmt, ...)
	__attribute__ ((format(printf, 2, 3)));

/* The first argument to do_log is a combination of the flags
 * 1 = print on statusline, 2 = more info in "errno", 4 = exit
 * Don't even look at these or the OS X build will cease to function.
 */
#if __GNUC__
# define DEBUG(fmt...)			do_log(0, fmt)
# define message(fmt...)		do_log(1, fmt)
# define FATAL(fmt...)			do_log(7, fmt)
# define DIE(fmt...)			do_log(5, fmt)
# define ERROR(fmt...)			do_log(2, fmt)
# define error_message(fmt...)	do_log(3, fmt)
#else
# define DEBUG(...)				do_log(0, __VA_ARGS__)
# define message(...)			do_log(1, __VA_ARGS__)
# define FATAL(...)				do_log(7, __VA_ARGS__)
# define DIE(...)				do_log(5, __VA_ARGS__)
# define ERROR(...)				do_log(2, __VA_ARGS__)
# define error_message(...)		do_log(3, __VA_ARGS__)
#endif

#endif
