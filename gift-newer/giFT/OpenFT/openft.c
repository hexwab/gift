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

#define __OPENFT_C__

#include <signal.h>

#include "openft.h"

#include "netorg.h"

#include "share_comp.h"
#include "search.h"
#include "daemon.h"

#include "http.h"
#include "html.h"

#include "file.h"
#include "parse.h"

/*****************************************************************************/

#define MAINTAIN_LINKS_TIMER (2 * MINUTES)

/*****************************************************************************/

int openft_shutdown = FALSE;

/*****************************************************************************/

Config *openft_conf = NULL;

/* current class */
Connection *ft_self   = NULL;
Connection *http_bind = NULL;

/* giFT protocol, not really terribly useful once in OpenFT space, but save
 * anyway */
Protocol *openft_proto = NULL;

/* timer descriptor for connection maintenance */
static unsigned long ft_maint = 0;

/*****************************************************************************/

/* report back the progress of this upload chunk */
void ft_upload (Chunk *chunk, char *segment, size_t len)
{
	FT_Transfer *xfer;

	assert (chunk);

	xfer = FT_TRANSFER (chunk);

	chunk_write (chunk, segment, len);
}

/* report back the progress of this download chunk */
void ft_download (Chunk *chunk, char *segment, size_t len)
{
	int finishing;

	/* finishing means that after this chunk_write, the download will be
	 * complete.  the giFT protocol dictates that we must handle this in
	 * protocol space, as the data we have right now will be freed once
	 * chunk_write is called */
	finishing = (chunk->start + chunk->transmit + len >= chunk->stop);

	if (finishing)
	{
		http_cancel (chunk);
		/* WARNING!  xfer is free'd now! */
	}

	/* write the data...
	 * NOTE: if finishing is true, all of this data will be freed once this
	 * is executed */
	chunk_write (chunk, segment, len);
}

/*****************************************************************************/

/* daemon has requested we download a file
 * TODO -- this can fail, but returns void? */
static void ft_daemon_download (Chunk *chunk, ProtocolCommand command,
                                void *data)
{
	FT_Transfer   *xfer;
	char          *head, *source;
	char          *ip, *request;
	unsigned short port;
	int            indirect;
	float          factor;

	switch (command)
	{
	 case PROTOCOL_TRANSFER_CANCEL:	/* giFT wants us to cancel this transfer */
		/* this is a sanity check now.. is it needed?? */
		if (!chunk->source)
			http_cancel (chunk);

		break;
	 case PROTOCOL_TRANSFER_START:
		/* we need to evaluate the protocol-specific source-line in the form:
		 *
		 * PULL:
		 * http://node:port/requested/file
		 *
		 *  - or -
		 *
		 * PUSH:
		 * http://searchnode:port/?local_port=myport&requested_host=node&requested_file=/requested/file
		 */

		/* TODO - error checking */

		head = source = STRDUP (chunk->source->url);

		TRACE (("source = %s", source));

		/*        */ string_sep (&source, "://"); /* push past http:// */
		ip   =       string_sep (&source, ":");   /* push past 127.0.0.1: */
		port = ATOI (string_sep (&source, "/"));  /* push past 1216/ */

		if (!source || !source[0])
		{
			free (head);
			return;
		}

		/* indirect download? ... this holds a temporary state in
		 * http_pull_file and will ignore the redirection successful page,
		 * waiting for the new connection to come back */
		indirect = (*source == '?') ? TRUE : FALSE;

		/* rewind source to see the leading / on the request, sigh...just
		 * easier :/ */
		source--;
		*source = '/';
		request = url_decode (source); /* this is a hack, its just going to get
		                                * encoded again in http_transfer_new */

		/* setup the FT_Transfer structure to attach */
		xfer = http_transfer_new (ft_download, inet_addr (ip), port,
		                          request, chunk->start + chunk->transmit,
		                          chunk->stop);

		chunk->data = xfer;

		free (request);

		http_pull_file (chunk, source, indirect);

		free (head);

		break;
	 case PROTOCOL_CHUNK_SUSPEND:
		xfer = FT_TRANSFER(chunk);
		if (xfer && xfer->c)
		{
			input_suspend (xfer->c);
			if (data)
			{
				factor = *((float *)data);
				net_sock_adj_buf (xfer->c->fd, SO_RCVBUF, factor);
			}
		}
		else
			TRACE (("no connection: suspend chunk %x: tick_cap=%d",
			        (unsigned int)chunk, chunk->tick_cap));

		break;
	 case PROTOCOL_CHUNK_RESUME:
		xfer = FT_TRANSFER (chunk);
		if (xfer && xfer->c)
			input_resume (xfer->c);
		else
			TRACE (("no connection: resume chunk=%x tick_cap=%d ",
			        (unsigned int)chunk, chunk->tick_cap));

		break;
	 default:
		break; /* The caller didn't want us to do anything. Weird eh? */
	}
}

/*****************************************************************************/

/* used to cancel uploads */
static void ft_daemon_upload (Chunk *chunk, ProtocolCommand command,
                              void *data)
{
	FT_Transfer   *xfer;
	float         factor;

	switch(command)
	{
	 case PROTOCOL_TRANSFER_CANCEL:	/* giFT wants us to cancel this transfer */
	 	if (chunk->source)
		{
			TRACE (("chunk->source = %s!?!?", chunk->source->url));
			return;
		}

		http_cancel (chunk);

		break;
	 case PROTOCOL_CHUNK_SUSPEND:
		xfer = FT_TRANSFER (chunk);
		if (xfer && xfer->c)
		{
			input_suspend (xfer->c);
			if (data)
			{
				factor = *((float *)data);
				net_sock_adj_buf (xfer->c->fd, SO_SNDBUF, factor);
			}
		}
		else
		{
			TRACE (("no connection: suspend chunk %x: tick_cap=%d",
			        (unsigned int)chunk, chunk->tick_cap));
		}

		break;
	 case PROTOCOL_CHUNK_RESUME:
		xfer = FT_TRANSFER (chunk);
		if (xfer && xfer->c)
			input_resume (xfer->c);
		else
		{
			TRACE (("no connection: resume chunk=%x tick_cap=%d ",
			        (unsigned int)chunk, chunk->tick_cap));
		}

		break;
	 default:
		break; /* The caller didn't want us to do anything. Weird eh? */
	}
}

/*****************************************************************************/

/* handle share actions for giFT */
static void ft_daemon_share (FileShare *file, ProtocolCommand command)
{
	switch (command)
	{
	 case PROTOCOL_SHARE_ADD:
		ft_share_local_add (file);
		break;
	 case PROTOCOL_SHARE_REMOVE:
		ft_share_local_remove (file);
		break;
	 case PROTOCOL_SHARE_FLUSH:
		ft_share_local_flush ();
		break;
	 case PROTOCOL_SHARE_SYNC:
		ft_share_local_sync ();
		break;
	 default:
		break;
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
	           (InputCallback) http_handle_incoming, FALSE);

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

static int conn_sort_vit (Connection *a, Connection *b)
{
	if (NODE (a)->vitality > NODE (b)->vitality)
		return -1;
	else if (NODE (a)->vitality < NODE (b)->vitality)
		return 1;

	return 0;
}

static void ft_initial_connect ()
{
	Connection *c;
	FILE *f;
	char *buf = NULL;
	char *ptr;

	/* start the initial connections */
	f = fopen (gift_conf_path ("OpenFT/nodes"), "r");

	/* try the global nodes file */
	if (!f)
	{
		char *filename;

		if (!(filename = malloc (strlen (platform_data_dir ()) + 50)))
			return;

		sprintf (filename, "%s/OpenFT/nodes", platform_data_dir ());

		f = fopen (filename, "r");

		free (filename);
	}

	if (!f)
	{
		GIFT_WARN (("unable to locate a nodes file...sigh"));
		return;
	}

	while (file_read_line (f, &buf))
	{
		unsigned long  vitality;
		char          *ip;
		unsigned short port;
		unsigned short http_port;

		ptr = buf;

		/* [vitality] [ip]:[port] [http_port] */

		vitality  = ATOUL (string_sep (&ptr, " "));
		ip        =        string_sep (&ptr, ":");
		port      = ATOI  (string_sep (&ptr, " "));
		http_port = ATOI  (string_sep (&ptr, " "));

		if (!ip)
			continue;

		c = node_register (inet_addr (ip), port, http_port, NODE_NONE, FALSE);

		if (!c)
			continue;

		NODE (c)->vitality = vitality;
	}

	if (!conn_length (NODE_NONE, -1))
	{
		GIFT_WARN (("No nodes loaded.  If you believe this is in error, try "
		            "removing your local nodes file at %s.  This will cause "
		            "this module to repopulate its list from the global "
		            "cache.",
		            gift_conf_path ("OpenFT/nodes")));
	}

	conn_sort ((CompareFunc) conn_sort_vit);

	fclose (f);
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

	/* p->name = strdup ("OpenFT"); */

	dataset_insert (p->support, "range-get", TRUE);

	/* setup the symbols for the daemon to call */
	p->callback = ft_daemon_callback;
	p->destroy  = ft_daemon_destroy;
	p->download = ft_daemon_download;
	p->upload   = ft_daemon_upload;
	p->share    = ft_daemon_share;
	p->dc_close = ft_daemon_close;

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

	ft_initial_connect ();
	node_maintain_links (NULL);

	return TRUE;
}
