/*
 * $Id: as_source.c,v 1.17 2005/09/15 21:13:53 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

/* create new source */
ASSource *as_source_create ()
{
	ASSource *source;

	if (!(source = malloc (sizeof (ASSource))))
		return NULL;

	source->host = INADDR_NONE;
	source->port = 0;
	source->inside_ip = INADDR_NONE;
	source->shost = INADDR_NONE;
	source->sport = 0;
	source->username = NULL;
	source->netname = NULL;
	source->parent_host = INADDR_NONE;
	source->parent_port = 0;

	return source;
}

/* create copy of source */
ASSource *as_source_copy (ASSource *source)
{
	ASSource *cpy;

	if (!(cpy = as_source_create ()))
		return NULL;

	cpy->host = source->host;
	cpy->port = source->port;
	cpy->shost = source->shost;
	cpy->sport = source->sport;
	cpy->username = gift_strdup (source->username);
	cpy->netname = gift_strdup (source->netname);
	cpy->parent_host = source->parent_host;
	cpy->parent_port = source->parent_port;

	return cpy;
}

/* free source */
void as_source_free (ASSource *source)
{
	if (!source)
		return;

	free (source->username);
	free (source->netname);
	free (source);
}

/*****************************************************************************/

/* returns TRUE if the sources are the same */
as_bool as_source_equal (ASSource *a, ASSource *b)
{
	if (!a || !b)
		return FALSE;

	return (a->host        == b->host  &&
	        a->port        == b->port  &&
/*
 * User on different supernodes or result from different supernodes is
 * still the same user.
 * Supernode uniquefies username by appending a hex number and giving it back
 * to us.
 */
#if 0
	        a->shost       == b->shost &&
	        a->sport       == b->sport &&
	        a->parent_host == b->parent_host &&
	        a->parent_port == b->parent_port &&
#endif
			((a->username == NULL && b->username == NULL) ||
	        (gift_strcmp (a->username, b->username) == 0)));
}

/* returns TRUE if the source is firewalled */
as_bool as_source_firewalled (ASSource *source)
{
	/* Note: There is really no way to tell if a source is _not_
	 * firewalled.
	 */
	return (!net_ip_routable (source->host)) || (source->port == 0);
}

/* returns TRUE if the source has enough info to send a push */
as_bool as_source_has_push_info (ASSource *source)
{
	return (net_ip_routable (source->shost) && source->sport != 0 &&
	        source->host != 0 && source->host != INADDR_NONE);
}

/*****************************************************************************/

#ifdef GIFT_PLUGIN

/* create source from gift url */
ASSource *as_source_unserialize (const char *str)
{
	ASSource *source;
	in_addr_t host, shost;
	int port, sport;
	char username[32] = "", host_str[20], shost_str[20];

	if (sscanf (str, "Ares:?host=%16[0-9.]&port=%d&shost=%16[0-9.]&sport=%d&username=%30s", host_str, &port, shost_str, &sport, username) < 4) /* username may be blank */
		return NULL;

#if 0
	AS_HEAVY_DBG_5 ("p: %s %d %s %d %s", host_str, port, shost_str, sport,
	                username);
#endif

	if (!(host = net_ip (host_str)) || !(shost = net_ip (shost_str)))
		return NULL;

	if (!(source = as_source_create ()))
		return NULL;

	source->host     = host;
	source->port     = port;
	source->shost    = shost;
	source->sport    = sport;
	source->username = strdup (username);

	return source;
}

/* create url for gift. Caller frees returned string. */
char *as_source_serialize (ASSource *source)
{
	char host_str[32], shost_str[32];
	
	if (!net_ip_strbuf (source->host, host_str, sizeof(host_str)) ||
	    !net_ip_strbuf (source->shost, shost_str, sizeof(shost_str)))
		return NULL;

	return stringf_dup ("Ares:?host=%s&port=%d&shost=%s&sport=%d&username=%s",
			    host_str, (int)source->port,
			    shost_str, (int)source->sport,
			    STRING_NOTNULL (source->username));
}

#endif

/* Return static debug string with source data. Do not use for anything
 * critical because threading may corrupt buffer.
 */
char *as_source_str (ASSource *source)
{
	static char buf[1024];
	int len;

	len = snprintf (buf, sizeof (buf), "user: %s:%d, username: %.32s, ",
	                net_ip_str (source->host), source->port,
	                source->username);

	len += snprintf (buf + len, sizeof (buf) - len, "supernode: %s:%d, ",
	                 net_ip_str (source->shost), source->sport);

	len += snprintf (buf + len, sizeof (buf) - len, "parent node: %s:%d",
	                 net_ip_str (source->parent_host), source->parent_port);

	return buf;
}

/*****************************************************************************/
