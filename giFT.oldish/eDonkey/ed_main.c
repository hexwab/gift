/*
 * ed_main.c
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
#define __EDONKEY_C__

#include "ed_main.h"
#include "ed_daemon.h"

#include <signal.h>

Config *edonkey_conf = NULL;            /* ~/.giFT/gift.conf */

Protocol *edonkey_proto = NULL;         /* protocol plugin pointer */


static void ed_daemon_share (FileShare *file, ProtocolCommand command,
                             void *data)
{
	switch (command)
	{
	 case PROTOCOL_SHARE_ADD:
		break;
	 case PROTOCOL_SHARE_REMOVE:
		break;
	 case PROTOCOL_SHARE_SYNC:
		break;
	 case PROTOCOL_SHARE_FLUSH:
		break;
	 default:
		break;
	}
}

void ed_daemon_download (Chunk *chunk, ProtocolCommand command, void *data)
{
        switch (command)
        {
         case PROTOCOL_TRANSFER_CANCEL:
//		 download_cancel (chunk, data);
		 break;
	case PROTOCOL_TRANSFER_START:
//		download_start (chunk, data);
		break;
         case PROTOCOL_SOURCE_REMOVE:
//		 source_remove (chunk, data);
		 break;
         case PROTOCOL_CHUNK_SUSPEND:
//		 download_suspend (chunk, data); 
		 break;
         case PROTOCOL_CHUNK_RESUME:
//		 download_resume (chunk, data);
		 break;
         default:
		 break;
        }
}

void ed_daemon_upload (Chunk *chunk, ProtocolCommand command, void *data)
{
        switch(command)
        {
         case PROTOCOL_TRANSFER_CANCEL:
//		 upload_cancel (chunk, data);
		 break;
	case PROTOCOL_TRANSFER_REGISTER:
//		upload_register (chunk, data);
		break;
	case PROTOCOL_TRANSFER_UNREGISTER:
//		upload_unregister (chunk, data);
		break;
	case PROTOCOL_CHUNK_SUSPEND:
//		upload_suspend (chunk, data);
		break;
	case PROTOCOL_CHUNK_RESUME:
//		upload_resume (chunk, data);
		break;
         default:
		 break;
        }
}

int ed_daemon_source_cmp (Source *a, Source *b)
{
	return -1;
}

int ed_daemon_user_cmp (char *a, char *b)
{
	return -1;
}

static void ed_daemon_destroy (Protocol *p)
{
        TRACE_FUNC ();
}

static void setup_protocol (Protocol *p)
{
	assert (p != NULL);
	p->callback   =                    ed_daemon_callback;
	p->destroy    =                    ed_daemon_destroy;
	p->download   = (ProtocolCallback) ed_daemon_download;
	p->upload     = (ProtocolCallback) ed_daemon_upload;
	p->share      = (ProtocolCallback) ed_daemon_share;

	p->source_cmp =                    ed_daemon_source_cmp;
	p->user_cmp   =                    ed_daemon_user_cmp;
}

/*****************************************************************************/

#if 0
static Connection *http_start (unsigned short port)
{
	Connection *c;

	TRACE (("http_port = %hu", port));

	c = connection_new (openft_proto);

	if (port > 0)
	{
		c->fd = net_bind (port, FALSE);
		assert (c->fd >= 0);
	}

	input_add (openft_proto, c, INPUT_READ,
	           (InputCallback) http_server_incoming, FALSE);

	node_conn_set (ft_self, 0, -1, port, NULL);

	return c;
}
#endif

#if 0
static Connection *ft_start (unsigned short klass, unsigned short port)
{
	Connection *c;
	int fd = -1;

	/* firewalled users can not go beyond the user class */
	if (port == 0)
		klass = NODE_USER;

#ifndef USE_LIBDB
	if (klass & NODE_SEARCH)
	{
		GIFT_WARN (("dropping NODE_SEARCH status due to lacking libdb "
		            "support"));
	}

	klass &= ~NODE_SEARCH;
#endif /* !USE_LIBDB */

	TRACE (("class = %hu, port = %hu", klass, port));

	if (port > 0)
	{
		fd = net_bind (port, FALSE);
		assert (fd >= 0);
	}

	c = node_new (fd);

	input_add (openft_proto, c, INPUT_READ,
	           (InputCallback) ft_session_incoming, FALSE);

	node_state_set (c, NODE_CONNECTED);
	node_class_set (c, klass);
	node_conn_set  (c, 0, port, -1, NODE_ALIAS);

	if (!(NODE (c)->cap))
		NODE (c)->cap = dataset_new (DATASET_LIST);

	return c;
}
#endif

#if 0
static unsigned char *openft_hash_md5 (char *path, char *type, int *len)
{
	if (len)
		*len = 16;

	return md5_digest (path, 0);
}
#endif

/*****************************************************************************/

/*
 * OpenFT was parsed out of /path/to/libOpenFT.so
 */
int eDonkey_init (Protocol *p)
{
	int klass;
	int port;
	int http_port;

	TRACE_FUNC ();

	edonkey_proto = p;

	/* set giFT support data */
#if 0
	hash_algo_register (p, "MD5", (HashAlgorithm) openft_hash_md5);

	protocol_support (p, "range-get", TRUE);
	protocol_support (p, "user-browse", TRUE);
	protocol_support (p, "hash-unique", TRUE);
	protocol_support (p, "chat-user", FALSE);
	protocol_support (p, "chat-group", FALSE);
#endif
	/* setup the symbols for the daemon to call */
	setup_protocol (p);

	/* setup the configuration table */
	edonkey_conf = gift_config_new ("eDonkey");
	//	port        = config_get_int (edonkey_conf, "main/serverhost=");
#if 0
	/* startup the listening sockets */
	ft_self   = ft_start   (klass, port);
	http_bind = http_start (http_port);
#endif

#if 0
	/* setup the connection maintenance timer (2 minute intervals) */
	ft_maint = timer_add (MAINTAIN_LINKS_TIMER,
	                      (TimerCallback) node_maintain_links, NULL);
#endif
#if 0
	ed_server_connect ();
#endif

	return TRUE;
}
