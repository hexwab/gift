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
 * $Id: giftcurs.h,v 1.65 2003/11/16 16:15:35 weinholt Exp $
 */
#ifndef _GIFTCURS_H
#define _GIFTCURS_H

#if HAVE_CONFIG_H
# include <config.h>
#endif

/* Turn off debugging if told so */
#ifdef NDEBUG
# define G_DISABLE_ASSERT
# define G_DISABLE_CHECKS
#endif

#define G_DISABLE_DEPRECATED
#include <glib.h>

#ifndef G_HAVE_GINT64
# error You need support for 64-bit integers to compile giFTcurs
#endif

extern const char *server_host;
extern const char *server_port;	/* Allow symbolic port specification */
extern const char *profile_name;
extern gboolean verbose;

#ifdef WIDE_NCURSES
extern gboolean utf8;			/* Are we using UTF-8? */
extern gboolean fancy_utf8;		/* Use cool UTF-8 line drawing chars. */
#else
#define utf8 0
#define fancy_utf8 0
#endif

void graceful_death(void);

/* Stolen from gkrellm. */
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

typedef int (*CmpFunc) (const void *, const void *);

#ifdef G_HAVE_ISO_VARARGS
# define DEBUG(...)				g_log(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, __VA_ARGS__)
#elif defined(G_HAVE_GNUC_VARARGS)
# define DEBUG(fmt...)			g_log(G_LOG_DOMAIN, G_LOG_LEVEL_DEBUG, fmt)
#else
# error "Your compiler lacks support for variable argument macros"
#endif

/* These two are not present in glib 2.0 */
#ifndef G_UNLIKELY
# define G_UNLIKELY(x) (x)
#endif
#ifndef G_LIKELY
# define G_LIKELY(x) (x)
#endif

/* Tell gcc this does not return */
#define assert_not_reached() G_STMT_START{ g_assert_not_reached(); abort(); }G_STMT_END

#endif
