/*
 * log.h
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

#ifndef __LOG_H
#define __LOG_H

/*****************************************************************************/

/**
 * @file log.h
 *
 * @brief Log debugging, warning and error messages.
 *
 * These logging facilities provide greater control over the end output
 * location, whether it be to stdout/stderr, a simple log file, or even
 * syslog facilities.
 */

/*****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif /* __reallycrappylanguage */

/*****************************************************************************/

#ifdef HAVE_SYSLOG_H
# include <syslog.h>
#endif

#ifndef LOG_PERROR
# define LOG_PERROR 0x20
#endif

#ifndef LOG_PFX
# define LOG_PFX NULL
#endif

/*****************************************************************************/

typedef enum
{
	GLOG_NONE        = 0x01, /* turn logging off */
	GLOG_STDERR      = 0x02, /* log to stderr, the default */
	GLOG_STDOUT      = 0x04, /* log to stdout */
	GLOG_SYSLOG      = 0x08, /* log to syslog (*nix) or Event Log (Win32) */
	GLOG_FILE        = 0x10, /* log to the specified file */
	GLOG_DEBUGGER    = 0x20  /* log to debugger, if running within debugger */
} LogOptions;

/*****************************************************************************/

/* TODO -- move to config.h eventually: --enable-debug/--enable-release */
#define DEBUG

#define GIFT_INFO(args)   (log_info  args)
#define GIFT_WARN(args)   (log_warn  args)
#define GIFT_ERROR(args)  (log_error args)
#define GIFT_FATAL(args)  (log_fatal args), exit(0)
#define GIFT_ERRNO()      platform_errno ()
#define GIFT_STRERROR()   platform_error ()
#define GIFT_NETERROR()   platform_net_error ()

#ifdef DEBUG

# define GIFT_DEBUG(args)  (log_debug args)
# define TRACE(args)       \
    (log_trace_pfx (LOG_PFX, __FILE__, __LINE__, __PRETTY_FUNCTION__, NULL), \
     log_trace args)
# define TRACE_SOCK(args)  \
    (log_trace_pfx (LOG_PFX, __FILE__, __LINE__, __PRETTY_FUNCTION__, net_peer_ip (c ? c->fd : -1)), \
     log_trace args)
# define TRACE_FUNC()      TRACE (("entered"))
# define TRACE_MEM(ptr, len) \
	(log_dump_memory ((void*) (ptr), (unsigned long) (len)))
# define DEBUG_TAG 0x12345678U
# define VALIDATE(arg) (arg = DEBUG_TAG)
# define INVALIDATE(arg) (arg = ~DEBUG_TAG)
# define ASSERT_VALID(arg) assert(arg == DEBUG_TAG)
# define GLOG_DEBUG GLOG_DEBUGGER

#else /* !DEBUG */

# define GIFT_DEBUG(args)
# define TRACE(args)
# define TRACE_FUNC()
# define TRACE_MEM(ptr, len)
# define VALIDATE(arg)
# define INVALIDATE(arg)
# define ASSERT_VALID(arg)
# define GLOG_DEBUG

#endif /* DEBUG */

#ifdef DEBUG_STDERR
# define GLOG_DEFAULT GLOG_FILE | GLOG_STDERR
#else
# define GLOG_DEFAULT GLOG_FILE
#endif

#if defined (__GNUC__) && 0
# define GIFT_FMTATTR(f,v) __attribute__ ((__format__ (__printf__, f, v)))
#else
# define GIFT_FMTATTR(f,v)
#endif /* __GNUC__ */

/*****************************************************************************/

/**
 * Initialize the log subsystem
 *
 * @param options
 * @param ident
 * @param syslog_option
 * @param facility
 * @param log_file
 *
 * @retval TRUE  Successful initialization
 * @retval FALSE Unable to load logging subsystem
 */
int log_init (LogOptions options, char *ident, int syslog_option, int facility,
			  char *log_file);

/**
 * Cleanup the log subsystem
 */
void log_cleanup ();

/*
 * Usage:
 * GIFT_WARN (("cant open %s: %s", file, platform_error()));
 * or
 * GIFT_WARN (("cant open %s: %s", file, GIFT_STRERROR()));
 */
void log_info  (char *fmt, ...) GIFT_FMTATTR (1, 2);
void log_warn  (char *fmt, ...) GIFT_FMTATTR (1, 2);
void log_error (char *fmt, ...) GIFT_FMTATTR (1, 2);
void log_fatal (char *fmt, ...) GIFT_FMTATTR (1, 2);

#ifdef DEBUG

void log_debug     (char *fmt, ...) GIFT_FMTATTR (1, 2);
void log_trace_pfx (char *pfx, char *file, int line, char *func, char *extra);
void log_trace     (char *fmt, ...) GIFT_FMTATTR (1, 2);

/**
 * @brief Log a hexidecimal dump of memory
 *
 * This function is available only when debugging is enabled at compile
 * time.  Avoid usage if possible.
 *
 * @param ptr pointer to memory area
 * @param len length of memory to dump.
 */
void log_dump_memory (void *ptr, unsigned int len);

#endif /* DEBUG */

/*****************************************************************************/

#ifdef __cplusplus
}
#endif /* __reallycrappylanguage */

/*****************************************************************************/

#endif /* __LOG_H */
