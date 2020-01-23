/*
 * $Id: as_incoming.h,v 1.3 2005/11/08 20:17:32 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_INCOMING_H
#define __AS_INCOMING_H

/*****************************************************************************/

int as_incoming_http (ASHttpServer *server, TCPC *tcpcon,
                      ASHttpHeader *request);

int as_incoming_binary (ASHttpServer *server, TCPC *tcpcon, ASPacket *request);

int as_incoming_push (ASHttpServer *server, TCPC *tcpcon, String *buf);

/*****************************************************************************/

#endif /* __AS_INCOMING_H */
