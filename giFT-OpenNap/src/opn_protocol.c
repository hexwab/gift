/* giFT OpenNap
 * Copyright (C) 2003 Tilman Sauerbeck <tilman@code-monkey.de>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307, USA.
 */

#include "opn_opennap.h"
#include "opn_protocol_handlers.h"

static Handler handler_table[] = {
	{OPN_CMD_LOGIN_ERROR, OPN_HANDLER_FUNC(login_error)},
	{OPN_CMD_LOGIN_ACK, OPN_HANDLER_FUNC(login_ack)},
	{OPN_CMD_SEARCH_RESULT, OPN_HANDLER_FUNC(search_result)},
	{OPN_CMD_SEARCH_FINISHED, OPN_HANDLER_FUNC(search_finished)},
	{OPN_CMD_DOWNLOAD_ACK, OPN_HANDLER_FUNC(download_ack)},
	{OPN_CMD_STATS, OPN_HANDLER_FUNC(stats)},
	{OPN_CMD_ERROR, OPN_HANDLER_FUNC(error)},
	{OPN_CMD_PING, OPN_HANDLER_FUNC(ping)},
	{OPN_CMD_NONE, NULL}
};

/* optimized lookup method for quick access of handler func by command */
static HandlerFn handlers[OPN_CMD_NUM];

static void init_handlers()
{
	Handler *ptr;
	int i;

	/* clear the handlers array before we set our valid data */
	for (i = 0; i < OPN_CMD_NUM; i++)
		handlers[i] = NULL;

	/* iterate over the original data structure and build up the better
	 * optimized handlers lookup method
	 */
	for (ptr = handler_table; ptr->func; ptr++)
		handlers[ptr->cmd] = ptr->func;
}

BOOL opn_protocol_handle(OpnPacket *packet, void *udata)
{
	static BOOL initialized = FALSE;
	
	if (!packet || packet->cmd < OPN_CMD_NONE ||
	    packet->cmd > OPN_CMD_NUM)
		return FALSE;

	if (!initialized) {
		init_handlers();
		initialized = TRUE;
	}
	
	if (handlers[packet->cmd])
		(*handlers[packet->cmd])(packet->data, udata);
	else {
#ifdef OPENNAP_DEBUG
		printf("unhandled packet (cmd = %i)!\n", packet->cmd);
#endif
	}
	
	return TRUE;
}

