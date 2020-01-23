/*
 * $Id: aresdll.c,v 1.1 2005/12/18 16:43:38 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "aresdll.h"
#include "as_ares.h"

#include "ar_threading.h"
#include "ar_callback.h"
#include "ar_misc.h"
#include "ar_upload.h"

/*****************************************************************************/

/* Startup library and set callback. The downloads will be resumed, uploads
 * will be possible, etc. If logfile is not NULL it specifies the path the
 * logfile will be written to.
 */
as_bool AR_CC ar_startup (ARCallback callback, const char *logfile)
{
	ASLogger *logger;

	/* If this returns TRUE we already have an event system */
	if (ar_events_pause ())
	{
		AS_ERR ("Tried to startup ares dll twice.");
		ar_events_resume ();
		return FALSE;
	}

	/* setup logging */
	if (!(logger = as_logger_create ()))
	{
		return FALSE;
	}

	if (logfile && !as_logger_add_output (logger, logfile))
	{
		return FALSE;
	}
	AS_DBG ("Logging subsystem started");

	/* winsock init */
	if (!tcp_startup ())
	{
		AS_ERR ("Winsock init failed.");
		as_logger_free (NULL);
		return FALSE;
	}

	/* setup callback window with specified callback */
	if (!ar_create_callback_system (callback))
	{
		AS_ERR ("Failed to setup callback system.");
		tcp_cleanup ();
		as_logger_free (NULL);
		return FALSE;
	}

	/* setup event system */
	if (!ar_start_event_thread ())
	{
		AS_ERR ("Failed to start event thread");
		ar_destroy_callback_system ();
		tcp_cleanup ();
		as_logger_free (NULL);
		return FALSE;
	}

	/* interrupt event thread so we can safely proceed */
	ar_events_pause ();

	/* init ares lib */
	if (!as_init ())
	{
		AS_ERR ("Failed to init Ares subsystem");
		ar_events_resume ();
		ar_stop_event_thread ();
		ar_destroy_callback_system ();
		tcp_cleanup ();
		as_logger_free (NULL);
		return FALSE;
	}

	/* Set upload callbacks */
	ar_upload_add_callbacks ();

	ar_events_resume ();

	AS_DBG ("Startup complete");

	return TRUE;
}

/*
 * Shutdown everything. Downloads will be stopped and saved to to disk,
 * uploads will be stopped, etc.
 */
as_bool AR_CC ar_shutdown ()
{
	if (!ar_events_pause ())
		return FALSE;

	/* Remove upload callbacks */
	ar_upload_remove_callbacks ();

	/* save downloads to disk */
	as_downman_stop_all (AS->downman, TRUE);

	/* cleanup ares lib */
	as_cleanup ();

	ar_events_resume ();

	if (!ar_stop_event_thread ())
	{
		AS_WARN ("Failed to stop event thread");
	}

	AS_DBG ("Shutdown complete");

	/* remove callback window */
	ar_destroy_callback_system ();

	/* free logger */
	as_logger_free (NULL);

	/* cleanup winsock */
	tcp_cleanup ();

	return TRUE;
}

/*****************************************************************************/

/*
 * Connect to network, share files and allow searches.
 */
as_bool AR_CC ar_connect ()
{
	if (!ar_events_pause ())
		return FALSE;

	/* Set stats callback in ar_misc.c */
	as_netinfo_set_stats_cb (AS->netinfo, ar_stats_callback);

	if (AS->sessman->connections > 0)
	{
		AS_DBG ("Already connecting");
		ar_events_resume ();
		return FALSE;
	}

	AS_DBG ("Connecting");

	/* load nodes */
	if (!as_nodeman_load (AS->nodeman, "nodes"))
		AS_WARN ("Couldn't load nodes file");

	/* connect to 4 supernodes */
	as_sessman_connect (AS->sessman, 4);

	ar_events_resume ();
	return TRUE;
}

/* 
 * Disconnect from network. Searches are no longer possible but downloads and
 * already started uploads continue.
 */
as_bool AR_CC ar_disconnect ()
{
	if (!ar_events_pause ())
		return FALSE;

	if (AS->sessman->connections == 0)
	{
		AS_DBG ("Already disconnected");
		ar_events_resume ();
		return FALSE;
	}

	AS_DBG ("Disconnecting");

	/* connect to 0 supernodes */
	as_sessman_connect (AS->sessman, 0);

	/* save nodes */
	if (!as_nodeman_save (AS->nodeman, "nodes"))
		AS_WARN ("Couldn't save nodes file");

	/* Remove stats callback */
	as_netinfo_set_stats_cb (AS->netinfo, NULL);

	ar_events_resume ();
	return TRUE;
}

/*****************************************************************************/
