/*
 * $Id: as_log.c,v 1.8 2004/10/03 14:32:51 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

/* the evil global */
ASLogger *g_logger = NULL;

/*****************************************************************************/

/* allocate and init logger */
ASLogger *as_logger_create ()
{
	ASLogger *logger;
	int i;

	assert (g_logger == NULL);

	if (!(logger = malloc (sizeof (ASLogger))))
		return NULL;

	/* bad */
	g_logger = logger;

	for (i = 0; i < MAX_LOG_OUTPUTS; i++)
	{
		logger->outputs[i].name = NULL;
		logger->outputs[i].fp = NULL;
	}

	return logger;
}

/* free logger */
void as_logger_free (ASLogger *logger)
{
	int i;

	if (!logger)
		logger = g_logger;

	if (!logger)
		return;

	for (i = 0; i < MAX_LOG_OUTPUTS; i++)
	{
		if (logger->outputs[i].name)
		{
			free (logger->outputs[i].name);
			fclose (logger->outputs[i].fp);
		}
	}

	/* bad */
	if (logger == g_logger)
		g_logger = NULL;

	free (logger);
}

/*****************************************************************************/

/*
 * Add/remove logging output.
 * name is either:
 *   - a file name
 *   - "stdout"
 *   - "stderr"
 */
as_bool as_logger_add_output (ASLogger *logger, const char *name)
{
	int i;

	for (i = 0; i < MAX_LOG_OUTPUTS; i++)
	{
		if (!logger->outputs[i].name)
		{
			FILE *fp;

			if (strcmp (name, "stdout") == 0)
				fp = stdout;
			else if (strcmp (name, "stderr") == 0)
				fp = stderr;
			else
			{
				if (!(fp = fopen (name, "w")))
					return FALSE;
			}
			
			logger->outputs[i].name = strdup (name);
			logger->outputs[i].fp = fp;

			return TRUE;
		}
		else if (strcmp (logger->outputs[i].name, name) == 0)
		{
			/* already added */
			return TRUE;			
		}
	}

	return FALSE;
}

as_bool as_logger_del_output (ASLogger *logger, const char *name)
{
	int i;

	for (i = 0; i < MAX_LOG_OUTPUTS; i++)
	{
		if (logger->outputs[i].name &&
		    strcmp (logger->outputs[i].name, name) == 0)
		{
			free (logger->outputs[i].name);
		
			if (logger->outputs[i].fp != stdout &&
			    logger->outputs[i].fp != stderr )
			{
				fclose (logger->outputs[i].fp);
			}

			logger->outputs[i].name = NULL;
			logger->outputs[i].fp = NULL;

			return TRUE;
		}
	}

	return FALSE;
}

/*****************************************************************************/

static char *get_timestamp (char buf[16])
{
	time_t     now;
	struct tm *local;

	now = time (NULL);
	local = localtime (&now);

	strftime (buf, 16, "[%H:%M:%S]", local);

	return buf;
}

static const char *get_loglevel_str (int level)
{
	switch (level)
	{
	case AS_LOG_ERROR:       return "ERROR";
	case AS_LOG_WARNING:     return "WARNING";
	case AS_LOG_DEBUG:       return "DBG";
	case AS_LOG_HEAVY_DEBUG: return "HVY";
	}

	assert (0);

	return NULL;
}

/* Primary logging function. */
void as_logger_logv (ASLogger *logger,int level, const char *file,
                     int line, const char *fmt, va_list args)
{
	char buf[1024 * 4]; /* do we need more? */
	char time_buf[16];
	int written, i;

	if (!logger)
		return;

	written = snprintf (buf, sizeof (buf), "%s (%-16s %d) %s: ",
	                    get_timestamp (time_buf), file, line,
	                    get_loglevel_str (level));

	if (written < 0 || written >= sizeof (buf))
		return;

	written = vsnprintf (buf + written, sizeof (buf) - written, fmt, args);

	if (written < 0 || written >= sizeof (buf))
		return;

	/* send string to all outputs */
	for (i = 0; i < MAX_LOG_OUTPUTS; i++)
	{
		if (logger->outputs[i].name)
		{
			fprintf (logger->outputs[i].fp, "%s\n", buf);
			fflush (logger->outputs[i].fp);
		}
	}
}

/* Wrapper for as_logger_logv. */
void as_logger_log (ASLogger *logger, int level, const char *file,
                     int line, const char *fmt, ...)
{
	va_list args;

	va_start (args, fmt);
	as_logger_logv (logger, level, file, line, fmt, args);
	va_end (args);
}

/*****************************************************************************/

