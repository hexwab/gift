/*
 * $Id: as_netinfo.h,v 1.10 2006/02/20 01:25:58 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#ifndef __AS_NETINFO_H
#define __AS_NETINFO_H

/*****************************************************************************/

typedef struct as_net_info_t ASNetInfo;

/* Called whenever user, files, size or connect state changes */
typedef void (*ASNetInfoStatsCb) (ASNetInfo *info);

struct as_net_info_t
{
	unsigned int conn_want; /* number of supernode connections we want */
	unsigned int conn_have; /* number of supernode connections we have */
	unsigned int users;     /* users on network */
	unsigned int files;     /* files on network */
	unsigned int size;      /* total network size in GB */

	unsigned char *nick;    /* nickname from server (unique?) */

	as_uint8 guid[16];

	ASNetInfoStatsCb stats_cb;

	in_addr_t outside_ip;  /* our ip from the outside */
	in_port_t port;        /* port we're listening on */

	as_bool firewalled;    /* TRUE if we are firewalled (default is TRUE until
	                        * a message saying otherwise from supernode) */
};

/*****************************************************************************/

/* allocate and init network info */
ASNetInfo *as_netinfo_create ();

/* free manager */
void as_netinfo_free (ASNetInfo *info);

/*****************************************************************************/

/* set callback for stats changes */
void as_netinfo_set_stats_cb (ASNetInfo *info, ASNetInfoStatsCb stats_cb);

/*****************************************************************************/

/* handle connect state change */
void as_netinfo_handle_connect (ASNetInfo *info, unsigned int conn_want,
                                unsigned int conn_have);

/* handle stats packet */
as_bool as_netinfo_handle_stats (ASNetInfo *info, ASSession *session,
                                 ASPacket *packet);

/* handle outside ip packet */
as_bool as_netinfo_handle_ip (ASNetInfo *info, ASSession *session,
                              ASPacket *packet);

/* handle firewall status packet */
as_bool as_netinfo_handle_fwstatus (ASNetInfo *info, ASSession *session,
                                    ASPacket *packet);

/* handle nickname packet */
as_bool as_netinfo_handle_nick (ASNetInfo *info, ASSession *session,
                              ASPacket *packet);

/*****************************************************************************/

#endif /* __AS_NETINFO_H */
