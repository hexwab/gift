/*
 * $Id: ft_protocol.c,v 1.66 2003/05/29 09:37:41 jasta Exp $
 *
 * Main interface to handle OpenFT protocol command messages.  All the real
 * work is off-loaded onto the proto directory.
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

#include "ft_openft.h"

#include "ft_protocol.h"

#include "proto/ft_handshake.h"
#include "proto/ft_sharing.h"
#include "proto/ft_query.h"
#include "proto/ft_transfer.h"

/*****************************************************************************/

typedef void (*HandlerFn) (TCPC *c, FTPacket *packet);

/* optimized lookup method for quick access of handler func by command */
#define PROTOCOL_HANDLERS 512
static HandlerFn handlers[PROTOCOL_HANDLERS];
static int handlers_init = FALSE;

/*****************************************************************************/

/* data structure used to build the handlers array */
static struct handler_ent
{
	uint16_t   command;
	HandlerFn  func;
}
handler_table[] =
{
	{ FT_VERSION_REQUEST,   ft_version_request      },
	{ FT_VERSION_RESPONSE,  ft_version_response     },
	{ FT_NODEINFO_REQUEST,  ft_nodeinfo_request     },
	{ FT_NODEINFO_RESPONSE, ft_nodeinfo_response    },
	{ FT_NODELIST_REQUEST,  ft_nodelist_request     },
	{ FT_NODELIST_RESPONSE, ft_nodelist_response    },
	{ FT_NODECAP_REQUEST,   ft_nodecap_request      },
	{ FT_NODECAP_RESPONSE,  ft_nodecap_response     },
	{ FT_PING_REQUEST,      ft_ping_request         },
	{ FT_PING_RESPONSE,     ft_ping_response        },
	{ FT_SESSION_REQUEST,   ft_session_request      },
	{ FT_SESSION_RESPONSE,  ft_session_response     },

	{ FT_CHILD_REQUEST,     ft_child_request        },
	{ FT_CHILD_RESPONSE,    ft_child_response       },
	{ FT_ADDSHARE_REQUEST,  ft_addshare_request     },
	{ FT_ADDSHARE_RESPONSE, ft_addshare_response    },
	{ FT_REMSHARE_REQUEST,  ft_remshare_request     },
	{ FT_REMSHARE_RESPONSE, ft_remshare_response    },
	{ FT_MODSHARE_REQUEST,  ft_modshare_request     },
	{ FT_MODSHARE_RESPONSE, ft_modshare_response    },
	{ FT_STATS_REQUEST,     ft_stats_request        },
	{ FT_STATS_RESPONSE,    ft_stats_response       },

	{ FT_SEARCH_REQUEST,    ft_search_request       },
	{ FT_SEARCH_RESPONSE,   ft_search_response      },
	{ FT_BROWSE_REQUEST,    ft_browse_request       },
	{ FT_BROWSE_RESPONSE,   ft_browse_response      },

	{ FT_PUSH_REQUEST,      ft_push_request         },
	{ FT_PUSH_FORWARD,      ft_push_forward         },

	{ 0,                    NULL                    }
};

/*****************************************************************************/

static void init_handlers (void)
{
	int i;
	struct handler_ent *ptr;

	/* clear the handlers array before we set our valid data */
	for (i = 0; i < PROTOCOL_HANDLERS; i++)
		handlers[i] = NULL;

	/* iterate over the original data structure and build up the better
	 * optimized handlers lookup method */
	for (ptr = handler_table; ptr->func; ptr++)
		handlers[(int)ptr->command] = ptr->func;

	/* prevent this func from being called again */
	handlers_init = TRUE;
}

static HandlerFn get_handler (uint16_t cmd)
{
	if (cmd >= PROTOCOL_HANDLERS)
		return NULL;

	if (!handlers_init)
		init_handlers ();

	/* mmm, O(1) */
	return handlers[(int)cmd];
}

static BOOL handle_command (TCPC *c, FTPacket *packet)
{
	HandlerFn handler;
	uint16_t  cmd;

	cmd = ft_packet_command (packet);

	if (!(handler = get_handler (cmd)))
	{
		FT->DBGSOCK (FT, c, "no handler for cmd=0x%04x len=0x%04x",
		             packet->command, packet->len);

		return FALSE;
	}

	/*
	 * Move the work somewhere else... I really think we should be evaluating
	 * some kind of return value here, but I'm not really sure what we would
	 * do with it yet, so I guess we'll leave it as is for now.
	 */
	(*handler) (c, packet);

	return TRUE;
}

static void handle_stream_pkt (FTStream *stream, FTPacket *stream_pkt, TCPC *c)
{
	handle_command (c, stream_pkt);
}

static BOOL handle_stream (TCPC *c, FTPacket *packet)
{
	FTStream *stream;

	if (!(stream = ft_stream_get (c, FT_STREAM_RECV, packet)))
		return FALSE;

	/* retrieve each complete parsed packet individually and pass to
	 * handle_command independently */
	ft_stream_recv (stream, packet, (FTStreamRecv) handle_stream_pkt, c);

	if (stream->eof)
		ft_stream_finish (stream);

	return TRUE;
}

BOOL ft_protocol_handle (TCPC *c, FTPacket *packet)
{
	if (!c || !packet)
		return FALSE;

	/* stream messages need a special interface to read all raw messages out,
	 * so we will simply wrap it around handle_command indirectly */
	if (ft_packet_flags (packet) & FT_PACKET_STREAM)
		return handle_stream (c, packet);

	return handle_command (c, packet);
}
