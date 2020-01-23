/*
 * log.c
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

#include <stdio.h>
#include <stdarg.h>

#include "gift.h"
#include "log.h"

/*****************************************************************************/

#ifndef LOG_USER
# define LOG_USER (1 << 31)
#endif

#ifndef LOG_EMERG
# define LOG_EMERG   0     /* system is unusable */
#endif

#ifndef LOG_ALERT
# define LOG_ALERT   1     /* action must be taken immediately */
#endif

#ifndef LOG_CRIT
# define LOG_CRIT    2     /* critical conditions */
#endif

#ifndef LOG_ERR
# define LOG_ERR     3     /* error conditions */
#endif

#ifndef LOG_WARNING
# define LOG_WARNING 4     /* warning conditions */
#endif

#ifndef LOG_NOTICE
# define LOG_NOTICE  5     /* normal but significant condition */
#endif

#ifndef LOG_INFO
# define LOG_INFO    6     /* informational */
#endif

#ifndef LOG_DEBUG
# define LOG_DEBUG   7     /* debug-level messages */
#endif

/*****************************************************************************/

/* reduces code duplication in each log function */
#define LOG_FORMAT(pfx,fmt)                                           \
	char    buf[4096];                                                \
	size_t  buf_len = 0;                                              \
	va_list args;                                                     \
	                                                                  \
	assert (pfx != NULL);                                             \
	assert (fmt != NULL);                                             \
	                                                                  \
	if (pfx)                                                          \
		buf_len = snprintf (buf, sizeof (buf) - 1, "%s", pfx);        \
	                                                                  \
	va_start (args, fmt);                                             \
	vsnprintf (buf + buf_len, sizeof (buf) - buf_len - 1, fmt, args); \
	va_end (args);

/*****************************************************************************/

/* output log data */
static LogOptions   log_options = GLOG_STDERR; /* for platform_init errors */
static FILE        *log_fd      = NULL;
static FILE        *log_file_fd = NULL;

/* horribly non-threadsafe macro abuse */
static char *trace_file  = "";
static int   trace_line  = 0;
static char *trace_func  = "";
static char *trace_extra = NULL;

/*****************************************************************************/

int log_init (LogOptions options, char *ident, int syslog_option, int facility,
              char *log_file)
{
	/* unset previous settings */
	log_cleanup ();

	log_options = GLOG_DEBUG;
	log_options |= (options ? options : GLOG_STDERR);

	if (log_options & GLOG_STDERR)
		log_fd = stderr;

	if (log_options & GLOG_STDOUT)
		log_fd = stdout;

	if (log_options & GLOG_SYSLOG)
	{
#ifdef HAVE_SYSLOG_H
		openlog (ident, syslog_option, facility);
#elif defined(WIN32)
		/* TODO -- add ugly windows code to log to Event Log */
#endif /* WIN32 */
	}

	if ((log_options & GLOG_FILE) && log_file)
	{
		if (log_file_fd)
			fclose (log_file_fd);

		if (!(log_file_fd = fopen (log_file, "w+t"))) /* t=cr/lf in win32 */
		{
			fprintf (stderr, "Can't open %s: %s", log_file, GIFT_STRERROR ());
			return FALSE;
		}
		GIFT_INFO (("%s %s (%s %s) started", PACKAGE, VERSION, __DATE__,
					__TIME__));
	}

	return TRUE;
}

void log_cleanup ()
{
	log_fd = NULL;

	if (log_file_fd)
	{
		fclose (log_file_fd);
		log_file_fd = NULL;
	}

	if (log_options & GLOG_SYSLOG)
	{
#ifdef HAVE_SYSLOG_H
		closelog ();
#elif defined(WIN32)
		/* TODO -- add ugly windows code to log to Event Log */
#endif /* WIN32 */
	}

	log_options = GLOG_STDERR;
}

/*****************************************************************************/

/* TODO -- if not initialized, log_fd should be emulated as stderr here */
static void log_print (int priority, char *message)
{
	FILE *output_fd = NULL;            /* stderr/stdout */

	/* make sure high priority messages get spewed to the console anyway */
	if (log_fd)
		output_fd = log_fd;
	else if (priority <= LOG_CRIT)
		output_fd = stderr;

	if (output_fd)
	{
		fprintf (output_fd, "%s\n", message);
		fflush (output_fd);
	}

	if (log_options & GLOG_SYSLOG)
	{
#ifdef HAVE_SYSLOG_H
		syslog (priority, "%s", message);
#else
# if defined(WIN32)
		/* TODO -- add ugly windows code to log to Event Log */
# endif /* !WIN32 */
		/* just log to stderr */
		fprintf (stderr, "%s\n", message);
		fflush (stderr);
#endif /* !HAVE_SYSLOG_H */
	}

	if (log_file_fd)
	{
		fprintf (log_file_fd, "%s\n", message);
		fflush (log_file_fd);
	}

#ifdef DEBUG
# ifdef WIN32
	if (log_options & GLOG_DEBUGGER)
	{
		OutputDebugString (message);
		OutputDebugString ("\n");
	}
# endif
#endif
}

void log_info (char *fmt, ...)
{
	LOG_FORMAT ("", fmt);
	log_print (LOG_INFO, buf);
}

void log_warn (char *fmt, ...)
{
	LOG_FORMAT ("** gift-warning:  ", fmt);
	log_print (LOG_WARNING, buf);
}

void log_error (char *fmt, ...)
{
	LOG_FORMAT ("** gift-error:    ", fmt);
	log_print (LOG_ERR, buf);
}

void log_fatal (char *fmt, ...)
{
	LOG_FORMAT ("** gift-fatal:    ", fmt);
	log_print (LOG_CRIT, buf);
}

/*****************************************************************************/

#ifdef DEBUG

void log_debug (char *fmt, ...)
{
	LOG_FORMAT ("** gift-debug:    ", fmt);
	log_print (LOG_DEBUG, buf);
}

void log_trace_pfx (char *file, int line, char *func, char *extra)
{
# ifndef WIN32
	trace_file  = file;
# else /* WIN32 */
	/* MSVC prepends the directory name */
	trace_file  = strrchr (file, '\\') ? strrchr (file, '\\') + 1 : file;
# endif /* !WIN32 */

	trace_line  = line;
	trace_func  = func;

	free (trace_extra);
	trace_extra = STRDUP (extra);
}

void log_trace (char *fmt, ...)
{
	char     buf[4096];
	size_t   buf_len = 0;
	va_list  args;

	assert (fmt);

	buf_len += snprintf (buf + buf_len, sizeof (buf) - buf_len - 1,
	                     "%-12s:%-4i %s", trace_file, trace_line, trace_func);

	if (trace_extra)
	{
		buf_len += snprintf (buf + buf_len, sizeof (buf) - buf_len - 1,
		                     " (%s)", trace_extra);
	}

	buf_len += snprintf (buf + buf_len, sizeof (buf) - buf_len - 1, ": ");

	va_start (args, fmt);
	vsnprintf (buf + buf_len, sizeof (buf) - buf_len - 1, fmt, args);
	va_end (args);

	log_print (LOG_DEBUG, buf);
}

/* uhm, yuck */
void log_dump_memory (void *ptr, unsigned int len)
{
	unsigned int  i;
	char         *p = (char *)ptr;
	char          buf[256];
	char         *p2;

	p2 = buf;
	for (i = 0; i < len; i++)
	{
		unsigned char c;
		if ((i % 16) == 0)
			p2 += sprintf (p2, "%04x: ", i);
		c = p[i];
		p2 += sprintf (p2, "%02x ", c);
		if (((i + 1) % 16) == 0)
		{
			log_print (LOG_DEBUG, buf);
			p2 = buf;
		}
	}
	if (p2 != buf)
		log_print (LOG_DEBUG, buf);
}

#endif /* DEBUG */
