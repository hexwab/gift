/*
 * $Id: as_ares.c,v 1.25 2005/11/08 20:17:32 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

ASInstance *as_instance = NULL;	/* global library instance */

/*****************************************************************************/

/* Called when configured listening port changes. */
static as_bool port_change_cb (const ASConfVal *old_val,
                               const ASConfVal *new_val, void *udata)
{
	ASHttpServer *server;

	/* If new port is 0 stop listening. */
	if (new_val->data.i == 0)
	{
		as_http_server_free (AS->server);
		AS->server = NULL;
		AS->netinfo->port = 0;

		AS_WARN ("Removed http server when port was changed to 0");
		return TRUE; /* Accept value change. */
	}

	/* Try to create http server with new port. */
	if (!(server = as_http_server_create ((in_port_t)new_val->data.i,
	                              (ASHttpServerRequestCb)as_incoming_http,
		                          (ASHttpServerPushCb)as_incoming_push,
	                              (ASHttpServerBinaryCb)as_incoming_binary)))
	{
		AS_WARN_1 ("Failed to move http server to port %d", new_val->data.i);
		return FALSE; /* Refuse value change. */
	}

	/* We have a new server. Free the old and replace it. */
	as_http_server_free (AS->server);
	AS->server = server;
	AS->netinfo->port = AS->server->port;

	AS_DBG_1 ("Moved http server to port %u", AS->server->port);

	return TRUE; /* Accept value change. */
}

/*****************************************************************************/

/* Config value defaults. */

static ASConfVal default_conf[] =
{
	/* id, name, type, data, callback, udata */
	{ AS_LISTEN_PORT,                 "main/port",      AS_CONF_INT, {59049},
	                                  port_change_cb,   NULL },
	/* warning: this is changed below to avoid problems with union initialization */
	{ AS_USER_NAME,                   "main/username",  AS_CONF_STR, {0},
	                                  NULL,             NULL },
	{ AS_DOWNLOAD_MAX_ACTIVE,         NULL,             AS_CONF_INT, {6},
	                                  NULL,             NULL },
	{ AS_DOWNLOAD_MAX_ACTIVE_SOURCES, NULL,             AS_CONF_INT, {10},
	                                  NULL,             NULL },
	{ AS_UPLOAD_MAX_ACTIVE,           NULL,             AS_CONF_INT, {3},
	                                  NULL,             NULL },
	{ AS_SEARCH_TIMEOUT,              "search/timeout", AS_CONF_INT, {3*60},
	                                  NULL,             NULL }
};

/*****************************************************************************/

/* Create library instance and initialize it. There can only be one at a time
 * though.
 */
as_bool as_init ()
{
	assert (AS == NULL);
	if (AS)
		return FALSE;

	AS_DBG ("Initializing Ares library...");

	if (!(AS = malloc (sizeof (ASInstance))))
	{
		AS_ERR ("Insufficient memory.");
		return FALSE;
	}

	/* Start in defined state so as_cleanup works right. */
	AS->config       = NULL;
	AS->nodeman      = NULL;
	AS->sessman      = NULL;
	AS->netinfo      = NULL;
	AS->searchman    = NULL;
	AS->downman      = NULL;
	AS->upman        = NULL;
	AS->pushman      = NULL;
	AS->pushreplyman = NULL;
	AS->shareman     = NULL;
	AS->server       = NULL;

	/* Create config first so rest can use it. */
	if (!(AS->config = as_config_create ()))
	{
		AS_ERR ("Failed to create config object");
		as_cleanup ();
		return FALSE;
	}
	
	/* HACK: avoid union initialization problems */
	default_conf[1].data.s = "antares";

	/* Add default values */
	if (!as_config_add_values (AS->config, default_conf,
	                   sizeof (default_conf) / sizeof (default_conf[0])))
	{
		AS_ERR ("Failed to add default values to config");
		as_cleanup ();
		return FALSE;
	}

	if (!(AS->netinfo = as_netinfo_create ()))
	{
		AS_ERR ("Failed to create network info");
		as_cleanup ();
		return FALSE;
	}

	if (!(AS->nodeman = as_nodeman_create ()))
	{
		AS_ERR ("Failed to create node manager");
		as_cleanup ();
		return FALSE;
	}

	if (!(AS->sessman = as_sessman_create ()))
	{
		AS_ERR ("Failed to create session manager");
		as_cleanup ();
		return FALSE;
	}

	if (AS_CONF_INT (AS_LISTEN_PORT) != 0)
	{
		if (!(AS->server = as_http_server_create (
			      (in_port_t) AS_CONF_INT (AS_LISTEN_PORT),
			      (ASHttpServerRequestCb)as_incoming_http,
			      (ASHttpServerPushCb)as_incoming_push,
			      (ASHttpServerBinaryCb)as_incoming_binary
			      )))
		{
			AS_ERR_1 ("Failed to create server on port %d",
			          AS_CONF_INT (AS_LISTEN_PORT));
		}
		else
			/* Set port so we can use it for pushes, sharing, etc */
			AS->netinfo->port = AS->server->port;
	}
	else
	{
		AS->server = NULL;
		AS_WARN ("HTTP server not started (no port set)");
	}

	if (!(AS->searchman = as_searchman_create ()))
	{
		AS_ERR ("Failed to create search manager");
		as_cleanup ();
		return FALSE;
	}

	if (!(AS->shareman = as_shareman_create ()))
	{
		AS_ERR ("Failed to create share manager");
		as_cleanup ();
		return FALSE;
	}

	if (!(AS->pushman = as_pushman_create ()))
	{
		AS_ERR ("Failed to create push manager");
		as_cleanup ();
		return FALSE;
	}

	if (!(AS->pushreplyman = as_pushreplyman_create ()))
	{
		AS_ERR ("Failed to create push reply manager");
		as_cleanup ();
		return FALSE;
	}

#ifndef GIFT_PLUGIN
	if (!(AS->downman = as_downman_create ()))
	{
		AS_ERR ("Failed to create download manager");
		as_cleanup ();
		return FALSE;
	}
#endif

	if (!(AS->upman = as_upman_create ()))
	{
		AS_ERR ("Failed to create upload manager");
		as_cleanup ();
		return FALSE;
	}

	return TRUE;
}

/* Clean up library instance */
as_bool as_cleanup ()
{
	assert (AS != NULL);
	if (!AS)
		return FALSE;

	AS_DBG ("Cleaning up Ares library...");

	as_upman_free (AS->upman);
#ifndef GIFT_PLUGIN
	as_downman_free (AS->downman);
#endif
	as_pushman_free (AS->pushman); /* Don't free before downman */
	as_pushreplyman_free (AS->pushreplyman);
	as_searchman_free (AS->searchman);
	as_shareman_free (AS->shareman);
	as_sessman_free (AS->sessman);
	as_nodeman_free (AS->nodeman);
	as_netinfo_free (AS->netinfo);
	as_http_server_free (AS->server);
	as_config_free (AS->config); /* Free last. */

	free (AS);
	AS = NULL;

	return TRUE;
}

/* Start connecting, resume downloads, etc. */
as_bool as_start ()
{
	assert (AS != NULL);
	if (!AS)
		return FALSE;

	return TRUE;
}

/* Drop all connections, stop downloads, etc. */
as_bool as_stop ()
{
	assert (AS != NULL);
	if (!AS)
		return FALSE;

	return TRUE;
}

/*****************************************************************************/
