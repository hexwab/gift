/*
 * $Id: if_message.c,v 1.5 2003/03/20 05:01:12 rossta Exp $
 *
 * this is quite possibly the worst interface ive ever created
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

#include "if_message.h"

/*****************************************************************************/

static void message_wrap (TCPC *c, IFEvent *event, void *udata)
{
	Array      *data;
	IFMessageCB msg_cb;
	Protocol   *p;
	char       *msg;
	Interface  *cmd;

	if (!(data = if_event_data (event, "Message")))
		return;

	list (&data, &msg_cb, &p, NULL);

	if (!(msg = msg_cb (c, event, udata)))
		return;

	/* send the message to the client */
	if ((cmd = interface_new ("MESSAGE", msg)))
	{
		if_event_send (event, cmd);
		interface_free (cmd);
	}

	free (msg);
}

IFEvent *if_message_new (TCPC *c, Protocol *p,
                         IFMessageCB display_msg, void *udata)
{
	IFEvent *event;
	Array   *data;

	if (!(data = array (NULL)))
		return NULL;

	event = if_event_new (c, 0, IFEVENT_BROADCAST | IFEVENT_NOID,
	                      message_wrap, udata, NULL, NULL,
	                      "Message", data);

	if (!event)
	{
		free (data);
		return NULL;
	}

	push (&data, display_msg);
	push (&data, p);

	(*message_wrap) (c, event, udata);

	return event;
}

void if_message_finish (IFEvent *event)
{
	void *data;

	if ((data = if_event_data (event, "Message")))
		free (data);

	if_event_finish (event);
}

/*****************************************************************************/

static char *send_wrapper (TCPC *c, IFEvent *event, char *msg)
{
	return STRDUP (msg);
}

void if_message_send (TCPC *c, Protocol *p, char *message)
{
	IFEvent *event;

	event = if_message_new (c, p, (IFMessageCB)send_wrapper, message);
	if_message_finish (event);
}
