/*
 * $Id: fst_source.c,v 1.3 2004/12/28 15:53:23 mkern Exp $
 *
 * Copyright (C) 2003 giFT-FastTrack project
 * http://developer.berlios.de/projects/gift-fasttrack
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

#include "fst_fasttrack.h"
#include "fst_source.h"

/*****************************************************************************/

/*
 * Thanks to hipnod for this url parsing sweetness.
 */
typedef char * (*SerializeFunc)   (FSTSource *source);
typedef BOOL   (*UnserializeFunc) (FSTSource *source, const char *key,
                                   const char *value);

#define URL_OPT_SERIALIZE(name) \
    char *url_spew_##name (FSTSource *source)

#define URL_OPT_UNSERIALIZE(name) \
    BOOL url_parse_##name (FSTSource *source, const char *key, \
                           const char *value)

#define DECLARE_URL_OPT(name) \
    static URL_OPT_SERIALIZE(name); \
    static URL_OPT_UNSERIALIZE(name)

/*****************************************************************************/

static BOOL url_parse_old (FSTSource *source, const char *url);
static BOOL url_parse_new (FSTSource *source, const char *url);
static char *url_create_new (FSTSource *source);

DECLARE_URL_OPT(ip);
DECLARE_URL_OPT(port);
DECLARE_URL_OPT(snode_ip);
DECLARE_URL_OPT(snode_port);
DECLARE_URL_OPT(parent_ip);
DECLARE_URL_OPT(username);

/*****************************************************************************/

typedef struct
{
	const char        *key;      /* key in url (i.e. "port" in "port=6346") */
	SerializeFunc     serialize;
	UnserializeFunc   unserialize;
} FSTUrlOption;

/*
 * Options that can go in our source URL format.
 */
static FSTUrlOption fst_source_url_options[] =
{
	{ "ip",    url_spew_ip,          url_parse_ip           },
	{ "port",  url_spew_port,        url_parse_port         },
	{ "sip",   url_spew_snode_ip,    url_parse_snode_ip     },
	{ "sport", url_spew_snode_port,  url_parse_snode_port   },
	{ "pip",   url_spew_parent_ip,   url_parse_parent_ip    },
	{ "user",  url_spew_username,    url_parse_username     },
	{ NULL, NULL, NULL }
};

static FSTUrlOption *lookup_url_option (const char *key)
{
	FSTUrlOption *opt = fst_source_url_options;

	for (; opt->key != NULL; opt++)
		if (strcmp (opt->key, key) == 0)
			return opt;

	return NULL;
}

/*****************************************************************************/

static void source_clear (FSTSource *source)
{
	source->ip         = 0;
	source->port       = 0;
	source->snode_ip   = 0;
	source->snode_port = 0;
	source->parent_ip  = 0;

	free (source->username);
	source->username = NULL;

	free (source->netname);
	source->netname = NULL;
	
	source->bandwidth = 0;
}

/* create new source object */
FSTSource *fst_source_create ()
{
	FSTSource *source;

	if (!(source = malloc (sizeof (FSTSource))))
		return NULL;

	source->username = NULL;
	source->netname = NULL;
	source_clear (source);

	return source;
}

/* create new source object with the data from org_source */
FSTSource *fst_source_create_copy (FSTSource *org_source)
{
	FSTSource *source;

	if (!(source = fst_source_create ()))
		return NULL;

	source->ip         = org_source->ip;
	source->port       = org_source->port;
	source->snode_ip   = org_source->snode_ip;
	source->snode_port = org_source->snode_port;
	source->parent_ip  = org_source->parent_ip;

	source->username = STRDUP (org_source->username);
	source->netname = STRDUP (org_source->netname);

	source->bandwidth  = org_source->bandwidth;

	return source;
}

/* create new source object from url */
FSTSource *fst_source_create_url (const char *url)
{
	FSTSource *source;

	if (!(source = fst_source_create ()))
		return NULL;

	if (!fst_source_decode (source, url))
	{
		fst_source_free (source);
		return NULL;
	}
	
	return source;
}

/* free source object */
void fst_source_free (FSTSource *source)
{
	if (!source)
		return;

	source_clear (source);
	free (source);
}

/*****************************************************************************/

/* parse an url */
BOOL fst_source_decode (FSTSource *source, const char *url)
{
	if (!source || !url)
		return FALSE;

	if (!strncmp (url, "FastTrack://", strlen ("FastTrack://")))
	{
		/* old url format */
		return url_parse_old (source, url);
	}
	else if (!strncmp (url, "FastTrack:?", strlen ("FastTrack:?")))
	{
		/* new url format */
		return url_parse_new (source, url);
	}

	return FALSE;
}

/* create an url, caller frees result */
char *fst_source_encode (FSTSource *source)
{
	/* always produce new urls */
	return url_create_new (source);
}

/*****************************************************************************/

/* returns TRUE if the sources are the same */
BOOL fst_source_equal (FSTSource *a, FSTSource *b)
{
	if (!a || !b)
		return FALSE;

	return (a->ip         == b->ip &&
	        a->port       == b->port &&
	        a->snode_ip   == b->snode_ip &&
	        a->snode_port == b->snode_port &&
#if 0
	        a->parent_ip  == b->parent_ip &&
#endif
	        !gift_strcmp (a->username, b->username));
}

/* returns TRUE if the source is firewalled */
BOOL fst_source_firewalled (FSTSource *source)
{
	assert (source);

	return (!fst_utils_ip_routable (source->ip) ||
	        source->port == 0);
}

/* returns TRUE if the source has enough info to send a push */
BOOL fst_source_has_push_info (FSTSource *source)
{
	assert (source);

	return (fst_utils_ip_routable (source->snode_ip) &&
	        source->snode_port != 0 &&
	        fst_utils_ip_routable (source->parent_ip) &&
			!string_isempty (source->username));
}

/*****************************************************************************/

/*
 * Old url format:
 * FastTrack://<host>:<port>/<base64 hash>?shost=<ip>&sport=<port>&uname=<username>
 */
static BOOL url_parse_old (FSTSource *source, const char *url)
{
	char *url0, *param_str;
	char *ip_str, *port_str;

	/* clear old stuff */
	source_clear (source);

	/* work on copy */
	url0 = param_str = STRDUP (url);

	string_sep (&param_str, "://");

	/* separate ip and port */
	if (! (port_str = string_sep (&param_str, "/")))
	{
		free (url0);
		return FALSE;
	}

	if (! (ip_str = string_sep (&port_str, ":")))
	{
		free (url0);
		return FALSE;
	}

	/* We only care for ip and port, the user has restarted to update so
	 * firewalled downloads won't work anyway.
	 */
	source->ip = net_ip (ip_str);
	source->port = ATOI (port_str);

	if (source->ip == 0 || source->ip == INADDR_NONE || source->port == 0)
	{
		source_clear (source);
		return FALSE;
	}

	free (url0);
	return TRUE;
}

/*
 * New url format:
 * FastTrack:?ip=<ip>&port=<port>&sip=<snode_ip>&sport=<snode_port>&...
 */
static BOOL url_parse_new (FSTSource *source, const char *url)
{
	char *option;
	char *key, *value;
	char *curl, *curl0;

	/* clear old stuff */
	source_clear (source);

	/* work on copy of url */
	curl = curl0 = STRDUP (url);

	/* skip prefix */
	string_sep (&curl, ":?");

	while ((option = string_sep (&curl, "&")))
	{
		FSTUrlOption *opt;

		value = option;
		key = string_sep (&value, "=");

		if (string_isempty (key) || string_isempty (value))
			continue;

		/* look up the key in our list of possible options */
		if ((opt = lookup_url_option (key)))
		{
			/* unserialize the specified key */
			if (!opt->unserialize (source, key, value))
			{
				/* The data for a recognized key was wrong.
				 * This url is broken */
				source_clear (source);
				free (curl0);
				return FALSE;
			}
		}
	}

	free (curl0);

	return TRUE;
}

static char *url_create_new (FSTSource *source)
{
	FSTUrlOption *opt;
	char *url, *value;
	size_t len;
	String str;

	string_init (&str);
	string_appendf (&str, "FastTrack:?");

	for (opt = fst_source_url_options; opt->key != NULL; opt++)
	{
		if ((value = opt->serialize (source)))
		{
			/* append value and separator for next argument */
			string_appendf (&str, "%s=%s&", opt->key, value);
		}
	}

	len = str.len;
	assert (len > 0);

	url = string_finish_keep (&str);

	/* remove trailing separator (may not be there if source is empty) */
	if (url[len - 1] == '&')
		url[len - 1] = 0;

	return url;
}

/*****************************************************************************/

/*
 * These functions return a static string or NULL if they produced no output.
 */

static URL_OPT_SERIALIZE (ip)
{
	if (source->ip == 0)
		return NULL;

	return net_ip_str (source->ip);
}

static URL_OPT_SERIALIZE (port)
{
	if (source->port == 0)
		return NULL;

	return stringf ("%d", source->port);
}

static URL_OPT_SERIALIZE (snode_ip)
{
	if (source->snode_ip == 0)
		return NULL;

	return net_ip_str (source->snode_ip);
}

static URL_OPT_SERIALIZE (snode_port)
{
	if (source->snode_port == 0)
		return NULL;

	return stringf ("%d", source->snode_port);
}

static URL_OPT_SERIALIZE (parent_ip)
{
	if (source->parent_ip == 0)
		return NULL;

	return net_ip_str (source->parent_ip);
}

static URL_OPT_SERIALIZE (username)
{
	static char static_name[64];
	char *name;

	if (string_isempty (source->username))
		return NULL;

	if (!(name = fst_utils_url_encode (source->username)))
		return FALSE;

	gift_strncpy (static_name, name, 63);
	free (name);

	return static_name;
}

/*****************************************************************************/

/*
 * These functions return TRUE if they were successful.
 */

static URL_OPT_UNSERIALIZE (ip)
{
	in_addr_t ip = net_ip (value);

	if (ip == 0 || ip == INADDR_NONE)
		return FALSE;

	source->ip = ip;
	return TRUE;
}

static URL_OPT_UNSERIALIZE (port)
{
	unsigned long port = gift_strtoul (value);

	if (port == ULONG_MAX || port >= 65536)
		return FALSE;

	source->port = port;
	return TRUE;
}

static URL_OPT_UNSERIALIZE (snode_ip)
{
	in_addr_t ip = net_ip (value);

	if (ip == 0 || ip == INADDR_NONE)
		return FALSE;

	source->snode_ip = ip;
	return TRUE;
}

static URL_OPT_UNSERIALIZE (snode_port)
{
	unsigned long port = gift_strtoul (value);

	if (port == ULONG_MAX || port >= 65536)
		return FALSE;

	source->snode_port = port;
	return TRUE;
}

static URL_OPT_UNSERIALIZE (parent_ip)
{
	in_addr_t ip = net_ip (value);

	if (ip == 0 || ip == INADDR_NONE)
		return FALSE;

	source->parent_ip = ip;

	return TRUE;
}

static URL_OPT_UNSERIALIZE (username)
{
	char *username;

	if (!(username = fst_utils_url_decode ((char *)value)))
		return FALSE;

	source->username = username;
	return TRUE;
}

/*****************************************************************************/

