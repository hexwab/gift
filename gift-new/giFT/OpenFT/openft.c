/*
 * openft.c
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

/* hack hack hack */
#define __OPENFT_C__

#include <signal.h>

#include "openft.h"

#include "netorg.h"

#include "share_comp.h"
#include "search.h"
#include "daemon.h"

#include "xfer.h"
#include "html.h"

#include "file.h"
#include "parse.h"

/*****************************************************************************/

#define MAINTAIN_LINKS_TIMER (2 * MINUTES)

/*****************************************************************************/

int openft_shutdown           = FALSE; /* boolean that is set when OpenFT is
                                        * shutting itself down */

/*****************************************************************************/

Config *openft_conf           = NULL;  /* ~/.giFT/gift.conf */

Connection *ft_self           = NULL;  /* current class information */
Connection *http_bind         = NULL;

Protocol *openft_proto        = NULL;  /* OpenFT protocol plugin pointer */

static unsigned long ft_maint = 0;     /* timer for connection maintenance */

/*****************************************************************************/

/* handle share actions for giFT */
static void ft_daemon_share (FileShare *file, ProtocolCommand command,
                             void *data)
{
	switch (command)
	{
	 case PROTOCOL_SHARE_ADD:    ft_share_local_add (file);    break;
	 case PROTOCOL_SHARE_REMOVE: ft_share_local_remove (file); break;
	 case PROTOCOL_SHARE_FLUSH:  ft_share_local_flush ();      break;
	 case PROTOCOL_SHARE_SYNC:   ft_share_local_sync ();       break;
	 default:                                                  break;
	}
}

/*****************************************************************************/

/* user has a closed a dc connection */
static void ft_daemon_close (Connection *c)
{
	TRACE_FUNC ();
}

/*****************************************************************************/

static Connection *cleanup_conn (Connection *c, Node *node, void *data)
{
	node_disconnect (c);

#if 0
	openft_share_remove_by_host (node->ip, TRUE);
#endif

	return NULL;
}

/* shutdown */
static void ft_daemon_destroy (Protocol *p)
{
	TRACE_FUNC ();

	/* boolean used by node_disconnect to disable certain behavior */
	openft_shutdown = TRUE;

	/* write nodes cache */
	node_update_cache ();

	/* disconnect */
	conn_clear ((ConnForeachFunc) cleanup_conn);

	/* cleanup */
	config_free (openft_conf);

	timer_remove (ft_maint);

	net_close (http_bind->fd);
	net_close (ft_self->fd);

	node_free (http_bind);
	node_free (ft_self);

	/* get rid of our protocol specific data */
	ft_share_local_cleanup ();
}

/*****************************************************************************/

static Connection *http_start (unsigned short port)
{
	Connection *c;

	TRACE (("http_port = %hu", port));

	c = connection_new (openft_proto);
	c->fd = net_bind (port, FALSE);

	input_add (openft_proto, c, INPUT_READ,
	           (InputCallback) http_server_incoming, FALSE);

	node_conn_set (ft_self, 0, -1, port);

	return c;
}

static Connection *ft_start (unsigned short klass, unsigned short port)
{
	Connection *c;

	/* firewalled users can not go beyond the user class */
	if (port == 0)
		klass = NODE_USER;

	TRACE (("class = %hu, port = %hu", klass, port));

	c = node_new (net_bind (port, FALSE));

	input_add (openft_proto, c, INPUT_READ,
	           (InputCallback) ft_handle_incoming, FALSE);

	node_state_set (c, NODE_CONNECTED);
	node_class_set (c, klass);
	node_conn_set  (c, 0, port, -1);

	if (!(NODE (c)->cap))
		NODE (c)->cap = dataset_new ();

	return c;
}

/*****************************************************************************/

/*
 * OpenFT was parsed out of /path/to/libOpenFT.so
 */
int OpenFT_init (Protocol *p)
{
	int klass;
	int port;
	int http_port;

	TRACE_FUNC ();

	openft_proto = p;

	/* set support */
	dataset_insert (p->support, "range-get",  I_PTR (TRUE));
	dataset_insert (p->support, "chat-user",  I_PTR (FALSE));
	dataset_insert (p->support, "chat-group", I_PTR (FALSE));

	/* setup the symbols for the daemon to call */
	p->callback   =                    ft_daemon_callback;
	p->destroy    =                    ft_daemon_destroy;
	p->download   = (ProtocolCallback) ft_daemon_download;   /* xfer.c */
	p->upload     = (ProtocolCallback) ft_daemon_upload;     /* xfer.c */
	p->source_cmp =                    ft_daemon_source_cmp; /* xfer.c */
	p->share      = (ProtocolCallback) ft_daemon_share;
	p->dc_close   =                    ft_daemon_close;

	/* setup the configuration table */
	openft_conf = gift_config_new ("OpenFT");
	port        = config_get_int (openft_conf, "main/port=1215");
	http_port   = config_get_int (openft_conf, "main/http_port=1216");
	klass       = config_get_int (openft_conf, "main/class=1");

	/* startup the listening sockets */
	ft_self   = ft_start   (klass, port);
	http_bind = http_start (http_port);

	/* add the capabilities */
#ifdef USE_ZLIB
	dataset_insert (NODE (ft_self)->cap, "ZLIB", STRDUP ("ZLIB"));
#endif /* USE_ZLIB */
	dataset_insert (NODE (ft_self)->cap, "MD5-FULL", STRDUP ("MD5-FULL"));

	/* setup the connection maintenance timer (2 minute intervals) */
	ft_maint = timer_add (MAINTAIN_LINKS_TIMER,
	                      (TimerCallback) node_maintain_links, NULL);

	/* make sure ~/.giFT/OpenFT/shares.gz is up to date */
	share_comp_write ();

	/* delete any host shares left over from previous search node sessions */
	file_rmdir (gift_conf_path ("OpenFT/db/"));

	/* OpenFT, here we come :) */
	node_maintain_links (NULL);

	return TRUE;
}
