/*
 * $Id: plugin.c,v 1.35 2003/05/04 06:55:49 jasta Exp $
 *
 * Copyright (C) 2001-2003 giFT project (gift.sourceforge.net)
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

#include "gift.h"

#include "protocol.h"
#include "if_event.h"

#include "event.h"
#include "plugin.h"

/* protocol functbl */
#include "share_hash.h"
#include "share_file.h"
#include "if_search.h"
#include "if_message.h"
#include "transfer.h"

/*****************************************************************************/

#ifndef USE_LTDL
# ifdef PLUGIN_OPENFT
int OpenFT_init (Protocol *p);
# endif /* PLUGIN_OPENFT */
# ifdef PLUGIN_GNUTELLA
int Gnutella_init (Protocol *p);
# endif /* PLUGIN_GNUTELLA */
#endif /* !USE_LTDL */

/*****************************************************************************/

/*
 * This is just terrible.  Please make me fix this.
 */
#define APPENDMSG msg + msgwr, sizeof (msg) - 1
#define LOGMSG(fmt,pfx1,pfx2)                                              \
	char    msg[4096];                                                     \
	size_t  msgwr = 0;                                                     \
	va_list args;                                                          \
	if (pfx1)                                                              \
		msgwr += snprintf (APPENDMSG, "%s: ", (char *)pfx1);               \
	if (pfx2)                                                              \
		msgwr += snprintf (APPENDMSG, "[%s]: ", (char *)pfx2);             \
	va_start (args, fmt);                                                  \
	vsnprintf (msg + msgwr, sizeof (msg) - msgwr - 1, fmt, args);          \
	va_end (args);

static int dbg_wrapper (Protocol *p, char *fmt, ...)
{
	LOGMSG (fmt, p->name, NULL);
	log_print (LOG_DEBUG, msg);

	return TRUE;
}

static int dbgsock_wrapper (Protocol *p, TCPC *c, char *fmt, ...)
{
	char hoststr[32];

	net_ip_strbuf (c->host, hoststr, sizeof (hoststr));

	{ /* ugh */
		LOGMSG (fmt, p->name, hoststr);
		log_print (LOG_DEBUG, msg);
	}

	return TRUE;
}

static int trace_wrapper (Protocol *p, char *file, int line, char *func,
						  char *fmt, ...)
{
	LOGMSG (fmt, NULL, NULL);

	return dbg_wrapper (p, "%s:%i(%s): %s", file, line, func, msg);
}

static int tracesock_wrapper (Protocol *p, TCPC *c, char *file, int line, char *func, char *fmt, ...)
{
	LOGMSG (fmt, NULL, NULL);

	return dbgsock_wrapper (p, c, "%s:%i(%s): %s", file, line, func, msg);
}

static int warn_wrapper (Protocol *p, char *fmt, ...)
{
	LOGMSG (fmt, p->name, NULL);
	log_warn ("%s", msg);

	return TRUE;
}

static int err_wrapper (Protocol *p, char *fmt, ...)
{
	LOGMSG (fmt, p->name, NULL);
	log_error ("%s", msg);

	return TRUE;
}

/*****************************************************************************/

static int hash_handler_wrapper (Protocol *p, const char *type, int opt,
                                 HashFn algofn, HashDspFn dspfn)
{
	return hash_algo_register (p, type, opt, algofn, dspfn);
}

static void support_wrapper (Protocol *p, char *feature, int spt)
{
	dataset_insert (&p->features, feature, STRLEN_0 (feature),
	                &spt, sizeof (spt));
}

static void search_result_wrapper (Protocol *p, IFEvent *event, char *user,
                                   char *node, char *href, unsigned int avail,
                                   FileShare *file)
{
	if_search_result (event, user, node, href, avail, file);
}

static void search_complete_wrapper (Protocol *p, IFEvent *event)
{
	if_search_remove (event, p);
}

static void chunk_write_wrapper (Protocol *p, Transfer *transfer,
                                 Chunk *chunk, Source *source,
                                 unsigned char *data, size_t len)
{
	chunk_write (chunk, data, len);
}

static void message_wrapper (Protocol *p, char *message)
{
	if_message_send (NULL, p, message);
}

/*****************************************************************************/

static void source_status_default (Protocol *p, Source *source,
                                   unsigned short klass, char *disptxt)
{
	source_status_set (source, (SourceStatus)klass, disptxt);
}

static int user_cmp_default (Protocol *p, char *a, char *b)
{
	return STRCMP (a, b);
}

static int source_cmp_default (Protocol *p, Source *a, Source *b)
{
	int ret;

	assert (a != NULL);
	assert (b != NULL);

	if ((ret = STRCMP (a->url, b->url)))
		return ret;

	if ((ret = STRCMP (a->hash, b->hash)))
		return ret;

	return user_cmp_default (p, a->user, b->user);
}

/*****************************************************************************/

static void setup_functbl (Protocol *p, int (*init) (Protocol *p))
{
	p->init            = init;
	p->trace           = trace_wrapper;
	p->tracesock       = tracesock_wrapper;
	p->dbg             = dbg_wrapper;
	p->dbgsock         = dbgsock_wrapper;
	p->warn            = warn_wrapper;
	p->err             = err_wrapper;
	p->hash_handler    = hash_handler_wrapper;
	p->support         = support_wrapper;
	p->search_result   = search_result_wrapper;
	p->search_complete = search_complete_wrapper;
	p->chunk_write     = chunk_write_wrapper;
	p->source_status   = source_status_default;
	p->source_cmp      = source_cmp_default;
	p->user_cmp        = user_cmp_default;
	p->message         = message_wrapper;
}

static Protocol *plugin_init (char *file, char *pname)
{
	Protocol    *p;
	int        (*init) (Protocol *p);
#ifdef USE_LTDL
	lt_dlhandle  handle;
#endif /* USE_LTDL */

	init = NULL;

#ifdef USE_LTDL
	if (!(handle = lt_dlopen (file)))
	{
		GIFT_FATAL (("couldn't load protocol in file %s: %s",
		             STRING_NOTNULL(file), lt_dlerror ()));
		return NULL;
	}

	init = lt_dlsym (handle, stringf ("%s_init", pname));
#else /* !USE_LTDL */
# ifdef PLUGIN_OPENFT
	if (!strcmp (pname, "OpenFT"))
		init = OpenFT_init;
# endif /* PLUGIN_OPENFT */
# ifdef PLUGIN_GNUTELLA
	if (!strcmp (pname, "Gnutella"))
		init = Gnutella_init;
# endif /* PLUGIN_GNUTELLA */
#endif /* USE_LTDL */

	if (!init)
	{
		char *error;

#ifdef USE_LTDL
		error = (char *)lt_dlerror ();
#else
		error = "binary does not support this protocol, consider ltdl support";
#endif

		GIFT_FATAL (("couldn't load protocol %s in file %s: %s",
		             pname, STRING_NOTNULL(file), error));

		return NULL;
	}

	/* create the protocol structure and store this data */
	if (!(p = protocol_new (pname)))
		return NULL;

#ifdef USE_LTDL
	p->handle = handle;
#endif /* USE_LTDL */

	setup_functbl (p, init);

	/* initialize the plugin */
	if (!(p->init (p)))
	{
		GIFT_FATAL (("%s (%s): init func failed", pname, file));

#ifdef USE_LTDL
		lt_dlclose (handle);
#endif /* LTDL */

		protocol_free (p);
		return NULL;
	}

	return p;
}

Protocol *plugin_load (char *file, char *pname)
{
	Protocol *p;
	int       ret;

	if (!pname)
	{
		GIFT_ERROR (("cannot load unnamed protocol from file %s",
		             STRING_NOTNULL(file)));
		return NULL;
	}

	/* call the plugin's init function */
	if ((p = plugin_init (file, pname)))
	{
		/* attempt to start the protocol's connections */
		if (!(ret = p->start (p)))
		{
			GIFT_FATAL (("unable to start '%s', no idea why (hopefully "
			             "the protocol provided some info)", p->name));
			protocol_free (p);
			return NULL;
		}
	}

	return p;
}

/*****************************************************************************/

void plugin_unload (Protocol *p)
{
	if (!p)
		return;

	/* get the protocol to kill itself */
	p->destroy (p);

#if 0
	/* remove all inputs from this protocol */
	input_remove_full (p, NULL, 0, NULL);
#endif

#ifdef USE_LTDL
	if (p->handle)
		lt_dlclose (p->handle);
#endif /* USE_LTLD */

	protocol_free (p);
}
