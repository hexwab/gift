/* giFTcurs - curses interface to giFT
 * Copyright (C) 2001-2002 G�ran Weinholt <weinholt@linux.nu>
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
 * $Id: giftcurs.h,v 1.40 2002/10/22 13:21:23 weinholt Exp $
 */
#ifndef _GIFTCURS_H
#define _GIFTCURS_H

#if HAVE_CONFIG_H
# include <config.h>
#endif

#ifndef __attribute__
# ifndef __GNUC__
#  define __attribute__(x)
# endif
#endif

extern char *server_host;
extern char *server_port;		/* Allow symbolic port specification */
extern char *profile_name;
extern char *download_dir;
extern int verbose;

void graceful_death(void);

/* Stolen from gkrellm.*/
#if defined (ENABLE_NLS)
# include <libintl.h>
# ifdef PACKAGE
#  undef _
#  define _(String) dgettext(PACKAGE,String)
# else
#  define _(String) gettext(String)
# endif							/* PACKAGE */
# ifdef gettext_noop
#  define N_(String) gettext_noop(String)
# else
#  define N_(String) (String)
# endif							/* gettext_noop */
#else
# define _(String) (String)
# define N_(String) (String)
# define textdomain(String) (String)
# define gettext(String) (String)
# define ngettext(single,plural,nr) ((nr)==1 ? (single) : (plural))
# define dgettext(Domain,String) (String)
# define dcgettext(Domain,String,Type) (String)
# define bindtextdomain(Domain,Directory) (Domain)
#endif

/* the lenght of a static buffer */
#ifndef buflen
# define buflen(buf) (sizeof buf/sizeof buf[0])
#endif

/* virtual methods helper macros */
#define IS_A(expr, type) (((type*)(expr))->methods == &type##_methods)
#define NEW(type) __extension__(({ type *t = calloc(1, sizeof(type)); t->methods = &type##_methods; t; }))

typedef int (*CmpFunc) (const void *, const void *);

#define __USE_GNU
#define _GNU_SOURCE

#include <stdarg.h>

#endif
