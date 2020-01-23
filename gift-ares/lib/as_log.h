/*
 * $Id: as_log.h,v 1.7 2004/11/28 17:01:19 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_LOG_H_
#define __AS_LOG_H_

/*****************************************************************************/

#ifndef GIFT_PLUGIN

#define MAX_LOG_OUTPUTS 4

typedef struct
{
	char *name;
	FILE *fp;

} ASLogOutput;

typedef struct
{
	ASLogOutput outputs[MAX_LOG_OUTPUTS];	

} ASLogger;

/*****************************************************************************/

/* allocate and init logger */
ASLogger *as_logger_create ();

/* free logger */
void as_logger_free (ASLogger *logger);

/*****************************************************************************/

/*
 * Add/remove logging output.
 * name is either:
 *   - a file name
 *   - "stdout"
 *   - "stderr"
 */
as_bool as_logger_add_output (ASLogger *logger, const char *name);

as_bool as_logger_del_output (ASLogger *logger, const char *name);

/*****************************************************************************/

/* Log levels */
#define AS_LOG_ERROR       0x01
#define AS_LOG_WARNING     0x02
#define AS_LOG_DEBUG       0x03
#define AS_LOG_HEAVY_DEBUG 0x04

/* Primary logging function. */
void as_logger_logv (ASLogger *logger, int level, const char *file,
                     int line, const char *fmt, va_list args);

/* Wrapper for as_logger_logv. */
void as_logger_log (ASLogger *logger, int level, const char *file,
                    int line, const char *fmt, ...);

/*****************************************************************************/

/* global logger instance from as_log.c */
extern ASLogger *g_logger;

/* oh well */
#  define AS_LOG(l) as_logger_log (g_logger, l, __FILE__, __LINE__


#else
#  define FILE_LINE_FUNC __FILE__,__LINE__,__PRETTY_FUNCTION__
#  define AS_LOG_DEBUG gift_proto->trace (gift_proto, FILE_LINE_FUNC
#  define AS_LOG_HEAVY_DEBUG gift_proto->trace (gift_proto, FILE_LINE_FUNC
#  define AS_LOG_ERROR gift_proto->err (gift_proto
#  define AS_LOG_WARNING gift_proto->warn (gift_proto
#  define AS_LOG(l) l
#endif

#ifdef DEBUG
#define AS_DBG(fmt)             AS_LOG (AS_LOG_DEBUG), fmt)
#define AS_DBG_1(fmt,a)         AS_LOG (AS_LOG_DEBUG), fmt,a)
#define AS_DBG_2(fmt,a,b)       AS_LOG (AS_LOG_DEBUG), fmt,a,b)
#define AS_DBG_3(fmt,a,b,c)     AS_LOG (AS_LOG_DEBUG), fmt,a,b,c)
#define AS_DBG_4(fmt,a,b,c,d)   AS_LOG (AS_LOG_DEBUG), fmt,a,b,c,d)
#define AS_DBG_5(fmt,a,b,c,d,e) AS_LOG (AS_LOG_DEBUG), fmt,a,b,c,d,e)
#else
#define AS_DBG(fmt)
#define AS_DBG_1(fmt,a)
#define AS_DBG_2(fmt,a,b)
#define AS_DBG_3(fmt,a,b,c)
#define AS_DBG_4(fmt,a,b,c,d)
#define AS_DBG_5(fmt,a,b,c,d,e)
#endif /* DEBUG */

#ifdef HEAVY_DEBUG
# define AS_HEAVY_DBG(fmt)               AS_LOG (AS_LOG_HEAVY_DEBUG), fmt)
# define AS_HEAVY_DBG_1(fmt,a)           AS_LOG (AS_LOG_HEAVY_DEBUG), fmt,a)
# define AS_HEAVY_DBG_2(fmt,a,b)         AS_LOG (AS_LOG_HEAVY_DEBUG), fmt,a,b)
# define AS_HEAVY_DBG_3(fmt,a,b,c)       AS_LOG (AS_LOG_HEAVY_DEBUG), fmt,a,b,c)
# define AS_HEAVY_DBG_4(fmt,a,b,c,d)     AS_LOG (AS_LOG_HEAVY_DEBUG), fmt,a,b,c,d)
# define AS_HEAVY_DBG_5(fmt,a,b,c,d,e)   AS_LOG (AS_LOG_HEAVY_DEBUG), fmt,a,b,c,d,e)
# define AS_HEAVY_DBG_6(fmt,a,b,c,d,e,f) AS_LOG (AS_LOG_HEAVY_DEBUG), fmt,a,b,c,d,e,f)
#else
# define AS_HEAVY_DBG(fmt)
# define AS_HEAVY_DBG_1(fmt,a)
# define AS_HEAVY_DBG_2(fmt,a,b)
# define AS_HEAVY_DBG_3(fmt,a,b,c)
# define AS_HEAVY_DBG_4(fmt,a,b,c,d)
# define AS_HEAVY_DBG_5(fmt,a,b,c,d,e)
# define AS_HEAVY_DBG_6(fmt,a,b,c,d,e,f)
#endif /* HEAVY_DEBUG */

#define AS_WARN(fmt)             AS_LOG (AS_LOG_WARNING), fmt)
#define AS_WARN_1(fmt,a)         AS_LOG (AS_LOG_WARNING), fmt,a)
#define AS_WARN_2(fmt,a,b)       AS_LOG (AS_LOG_WARNING), fmt,a,b)
#define AS_WARN_3(fmt,a,b,c)     AS_LOG (AS_LOG_WARNING), fmt,a,b,c)
#define AS_WARN_4(fmt,a,b,c,d)   AS_LOG (AS_LOG_WARNING), fmt,a,b,c,d)
#define AS_WARN_5(fmt,a,b,c,d,e) AS_LOG (AS_LOG_WARNING), fmt,a,b,c,d,e)

#define AS_ERR(fmt)              AS_LOG (AS_LOG_ERROR), fmt)
#define AS_ERR_1(fmt,a)          AS_LOG (AS_LOG_ERROR), fmt,a)
#define AS_ERR_2(fmt,a,b)        AS_LOG (AS_LOG_ERROR), fmt,a,b)
#define AS_ERR_3(fmt,a,b,c)      AS_LOG (AS_LOG_ERROR), fmt,a,b,c)
#define AS_ERR_4(fmt,a,b,c,d)    AS_LOG (AS_LOG_ERROR), fmt,a,b,c,d)
#define AS_ERR_5(fmt,a,b,c,d, e) AS_LOG (AS_LOG_ERROR), fmt,a,b,c,d,e)

/*****************************************************************************/

#endif /* __AS_LOG_H_ */
