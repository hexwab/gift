/*
 * $Id: source.c,v 1.12 2005/01/04 14:31:44 mkern Exp $
 *
 * Copyright (C) 2002-2003 giFT project (gift.sourceforge.net)
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

#include "gt_gnutella.h"
#include "gt_share_file.h"

#include "encoding/url.h"

#include "transfer/source.h"
#include "transfer/download.h"

/*****************************************************************************/

/*
 * Most of the goop in this file is for specifying each parameter for the giFT
 * source URL.  The source URL is supposed to encode all the information
 * necessary for contacting a source.
 */

/*****************************************************************************/

typedef BOOL (*UnserializeFunc) (GtSource *gt, const char *key,
                                 const char *value);
typedef BOOL (*SerializeFunc)   (GtSource *gt, String *s);

#define URL_OPT_SERIALIZE(name) \
    BOOL gt_src_spew_##name (GtSource *gt, String *s)

#define URL_OPT_UNSERIALIZE(name) \
	BOOL gt_src_parse_##name (GtSource *gt, const char *key, const char *value)

#define DECLARE_URL_OPT(name) \
    static URL_OPT_SERIALIZE(name); \
    static URL_OPT_UNSERIALIZE(name)

/*****************************************************************************/

DECLARE_URL_OPT(ip);
DECLARE_URL_OPT(port);
DECLARE_URL_OPT(server_ip);
DECLARE_URL_OPT(server_port);
DECLARE_URL_OPT(guid);
DECLARE_URL_OPT(fw);
DECLARE_URL_OPT(index);
DECLARE_URL_OPT(name);

/*
 * Options that can go in our source URL format.
 */
static struct url_option
{
	const char          *key;         /* key in url (i.e. "port" in "port=6346") */
	SerializeFunc        serialize;
	UnserializeFunc      unserialize;
} gt_source_url_options[] =
{
	{ "ip",    gt_src_spew_ip,          gt_src_parse_ip           },
	{ "port",  gt_src_spew_port,        gt_src_parse_port         },
	{ "sip",   gt_src_spew_server_ip,   gt_src_parse_server_ip    },
	{ "sport", gt_src_spew_server_port, gt_src_parse_server_port  },
	{ "fw",    gt_src_spew_fw,          gt_src_parse_fw           },
	{ "guid",  gt_src_spew_guid,        gt_src_parse_guid         },
	{ "index", gt_src_spew_index,       gt_src_parse_index        },
	{ "name",  gt_src_spew_name,        gt_src_parse_name         },
	{ NULL, NULL, NULL }
};

/*****************************************************************************/

/*
 * These functions return TRUE if they produced some output.
 */

static URL_OPT_SERIALIZE(ip)
{
	if (!gt->user_ip)
		return FALSE;

	string_appendf (s, "ip=%s", net_ip_str (gt->user_ip));
	return TRUE;
}

static URL_OPT_SERIALIZE(port)
{
	if (gt->user_port == 6346)
		return FALSE;

	string_appendf (s, "port=%hu", gt->user_port);
	return TRUE;
}

static URL_OPT_SERIALIZE(name)
{
	if (!gt->filename)
		return FALSE;

	string_appendf (s, "name=%s", gt->filename);
	return TRUE;
}

static URL_OPT_SERIALIZE(guid)
{
	if (gt_guid_is_empty (gt->guid))
		return FALSE;

	string_appendf (s, "guid=%s", gt_guid_str (gt->guid));
	return TRUE;
}

static URL_OPT_SERIALIZE(server_ip)
{
	if (!gt->server_ip)
		return FALSE;

	string_appendf (s, "sip=%s", net_ip_str (gt->server_ip));
	return TRUE;
}

static URL_OPT_SERIALIZE(server_port)
{
	if (gt->server_port == 6346)
		return FALSE;

	string_appendf (s, "sport=%hu", gt->server_port);
	return TRUE;
}

static URL_OPT_SERIALIZE(fw)
{
	if (!gt->firewalled)
		return FALSE;

	string_append (s, "fw=1");
	return TRUE;
}

static URL_OPT_SERIALIZE(index)
{
	if (!gt->index)
		return FALSE;

	string_appendf (s, "index=%u", gt->index);
	return TRUE;
}

/*****************************************************************************/

/*
 * These functions return TRUE if they were successful.
 */

static URL_OPT_UNSERIALIZE(ip)
{
	in_addr_t ip;

	ip = net_ip (value);

	if (ip == 0 || ip == INADDR_NONE)
		return FALSE;

	gt->user_ip = ip;
	return TRUE;
}

static URL_OPT_UNSERIALIZE(port)
{
	unsigned long port;

	port = gift_strtoul (value);

	if (port == ULONG_MAX || port >= 65536)
		return FALSE;

	gt->user_port = port;
	return TRUE;
}

static URL_OPT_UNSERIALIZE(name)
{
	char *name;

	if (!(name = STRDUP (value)))
		return FALSE;

	gt->filename = name;
	return TRUE;
}

static URL_OPT_UNSERIALIZE(guid)
{
	gt_guid_t *guid;

	if (!(guid = gt_guid_bin (value)))
		return FALSE;

	free (gt->guid);
	gt->guid = guid;

	return TRUE;
}

static URL_OPT_UNSERIALIZE(server_ip)
{
	in_addr_t ip;

	ip = net_ip (value);

	if (ip == 0 || ip == INADDR_NONE)
		return FALSE;

	gt->server_ip = ip;
	return TRUE;
}

static URL_OPT_UNSERIALIZE(server_port)
{
	unsigned long port;

	port = gift_strtoul (value);

	if (port == ULONG_MAX || port >= 65536)
		return FALSE;

	gt->server_port = port;
	return TRUE;
}

static URL_OPT_UNSERIALIZE(fw)
{
	unsigned long fw;

	fw = gift_strtoul (value);

	if (fw != 0 && fw != 1)
		return FALSE;

	if (fw)
		gt->firewalled = TRUE;
	else
		gt->firewalled = FALSE;

	return TRUE;
}

static URL_OPT_UNSERIALIZE(index)
{
	unsigned long index;

	index = gift_strtoul (value);

	if (index == ULONG_MAX)
		return FALSE;

	gt->index = (uint32_t)index;
	return TRUE;
}

/*****************************************************************************/

/*
 * Old Gnutella URL format:
 *
 * Gnutella://<u-ip>:<u-port>@<s-ip>:<s-port>[[FW]]:<client-guid>/<index>/<name>
 *
 * server_port is the server's gnutella port. This should probably pass
 * back both the gnutella port instead and the peer's connecting port, to
 * help in disambiguating different users behind the same firewall.
 */
static BOOL parse_old_url (char *url,
                           uint32_t *r_user_ip, uint16_t *r_user_port,
                           uint32_t *r_server_ip, uint16_t *r_server_port,
                           BOOL *firewalled, char **r_pushid,
                           uint32_t *r_index, char **r_fname)
{
	char *port_and_flags;
	char *flag;

	string_sep (&url, "://");

	/* TODO: check for more errors */

	*r_user_ip     = net_ip       (string_sep (&url, ":"));
	*r_user_port   = gift_strtoul (string_sep (&url, "@"));
	*r_server_ip   = net_ip       (string_sep (&url, ":"));

	/* handle bracketed flags after port. ugh, this is so ugly */
	port_and_flags = string_sep (&url, ":");
	*r_server_port = gift_strtoul (string_sep (&port_and_flags, "["));

	if (!string_isempty (port_and_flags))
	{
		/* grab any flags inside the brackets */
		while ((flag = string_sep_set (&port_and_flags, ",]")))
		{
			if (!STRCMP (flag, "FW"))
				*firewalled = TRUE;
		}
	}

	*r_pushid      =               string_sep (&url, "/");
	*r_index       = gift_strtoul (string_sep (&url, "/"));
	*r_fname       = url;

	return TRUE;
}

static struct url_option *lookup_url_option (const char *key)
{
	struct url_option *url_opt;

	url_opt = &gt_source_url_options[0];

	while (url_opt->key != NULL)
	{
		if (strcmp (url_opt->key, key) == 0)
			return url_opt;

		url_opt++;
	}

	return NULL;
}

/*
 * New parameter-based URL format:
 *
 * Gnutella:?ip=<u-ip>&port=<u-port>&sip=<s-ip>&sport=<s-port>[&fw=<FW>]...
 *
 * Parameters we don't understand are placed in gt_src->extra Dataset, so we
 * should be forwards and backwards compatible when adding new parameters.
 */
static BOOL parse_new_url (char *url, GtSource *gt)
{
	char *option;
	char *key;
	char *value;

	/* skip prefix */
	string_sep (&url, ":?");

	while ((option = string_sep (&url, "&")))
	{
		struct url_option *url_opt;

		value = option;
		key = string_sep (&value, "=");

		if (string_isempty (key) || string_isempty (value))
			continue;

		/* look up the key in our list of possible options */
		if ((url_opt = lookup_url_option (key)))
		{
			/* unserialize the specified key */
			if (url_opt->unserialize (gt, key, value))
				continue;

			/* fail through on failure to store failed keys */
		}

		/* store the unfound keys in the extra parameter dataset */
		dataset_insertstr (&gt->extra, key, value);
	}

	return TRUE;
}

/*****************************************************************************/

static GtSource *handle_old_url (char *url)
{
	GtSource  *gt;
	char      *fname      = NULL;
	char      *guid_ascii = NULL;

	if (!(gt = gt_source_new ()))
		return NULL;

	if (!parse_old_url (url, &gt->user_ip, &gt->user_port,
	                    &gt->server_ip, &gt->server_port,
	                    &gt->firewalled, &guid_ascii, &gt->index, &fname))
	{
		gt_source_free (gt);
		return NULL;
	}

	gt->filename = NULL;
	if (!string_isempty (fname))
		gt->filename = STRDUP (fname);

	gt->guid = NULL;
	if (!string_isempty (guid_ascii))
		gt->guid = gt_guid_bin (guid_ascii);

	return gt;
}

static GtSource *handle_new_url (char *url)
{
	GtSource *gt;

	if (!(gt = gt_source_new ()))
		return NULL;

	if (!parse_new_url (url, gt))
	{
		gt_source_free (gt);
		return NULL;
	}

	return gt;
}

GtSource *gt_source_unserialize (const char *url)
{
	char     *t_url;
	GtSource *src   = NULL;

	if (!url)
		return NULL;

	if (!(t_url = STRDUP (url)))
		return NULL;

	/*
	 * Determine whether this is the new format URL (beginning with
	 * "Gnutella:?") or the old-style (starts with "Gnutella://")
	 */
	if (strncmp (t_url, "Gnutella://", sizeof ("Gnutella://") - 1) == 0)
	{
		src = handle_old_url (t_url);
	}
	else if (strncmp (t_url, "Gnutella:?", sizeof ("Gnutella:?") - 1) == 0)
	{
		src = handle_new_url (t_url);
	}
	else
	{
		/* do nothing */
	}

	FREE (t_url);

	return src;
}

/* use the old format serialization for now */
#if 0
static void unknown_opt (ds_data_t *key, ds_data_t *value, void *udata)
{
	String *str = udata;
	string_appendf (str, "%s=%s&", key->data, value->data);
}

char *gt_source_serialize (GtSource *gt)
{
	struct url_option *opt;
	char              *url;
	size_t             len;
	String             str;

	string_init (&str);
	string_appendf (&str, "%s:?", GT->name);

	for (opt = gt_source_url_options; opt->key != NULL; opt++)
	{
		if (opt->serialize (gt, &str))
		{
			/* append separator for next argument */
			string_appendc (&str, '&');
		}
	}

	/* copy unknown options to the URL */
	dataset_foreach (gt->extra, unknown_opt, &str);

	len = str.len;
	assert (len > 0);

	url = string_finish_keep (&str);

	/* remove trailing separator (may not be there if source is empty) */
	if (url[len - 1] == '&')
		url[len - 1] = 0;

	return url;
}
#endif

/* serialize to the old format for now */
char *gt_source_serialize (GtSource *gt)
{
	String *str;

	if (!(str = string_new (NULL, 0, 0, TRUE)))
		return FALSE;

	string_appendf (str, "Gnutella://%s:%hu", net_ip_str (gt->user_ip),
	                gt->user_port);

	string_appendf (str, "@%s:%hu", net_ip_str (gt->server_ip),
	                gt->server_port);

	string_appendc (str, '[');

	if (gt->firewalled)
		string_append (str, "FW");

	string_appendc (str, ']');

	string_appendf (str, ":%s/%lu",
	                STRING_NOTNULL (gt_guid_str (gt->guid)), (long)gt->index);
	string_appendf (str, "/%s",
	                STRING_NOTNULL (gt->filename)); /* already encoded */

	return string_free_keep (str);
}

/*****************************************************************************/

/*
 * This is called by the search result code in order to produce
 * source URLs.
 *
 * TODO: This is just wrong -- interface is not very extensible.  The search
 * result code should probably use GtSource and call gt_source_serialize().
 */
char *gt_source_url_new (const char *filename, uint32_t index,
                         in_addr_t user_ip, uint16_t user_port,
                         in_addr_t server_ip, uint16_t server_port,
                         BOOL firewalled, const gt_guid_t *client_id)
{
	GtSource *src;
	char     *url;

	if (!(src = gt_source_new ()))
		return NULL;

	gt_source_set_ip          (src, user_ip);
	gt_source_set_port        (src, user_port);
	gt_source_set_index       (src, index);
	gt_source_set_server_ip   (src, server_ip);
	gt_source_set_server_port (src, server_port);
	gt_source_set_firewalled  (src, firewalled);

	if (!gt_source_set_guid (src, client_id) ||
	    !gt_source_set_filename (src, filename))
	{
		gt_source_free (src);
		return NULL;
	}

	url = gt_source_serialize (src);
	gt_source_free (src);

	return url;
}

/*****************************************************************************/

GtSource *gt_source_new (void)
{
	GtSource *src;

	if (!(src = NEW (GtSource)))
		return NULL;

	/* special case: port is 6346 if not specified */
	src->user_port   = 6346;
	src->server_port = 6346;

	return src;
}

void gt_source_free (GtSource *gt)
{
	if (!gt)
		return;

	free (gt->guid);
	free (gt->filename);
	free (gt->status_txt);

	FREE (gt);
}

/*****************************************************************************/

void gt_source_set_ip (GtSource *src, in_addr_t ip)
{
	src->user_ip = ip;
}

void gt_source_set_port (GtSource *src, in_port_t port)
{
	src->user_port = port;
}

void gt_source_set_index (GtSource *src, uint32_t index)
{
	src->index = index;
}

void gt_source_set_server_ip (GtSource *src, in_addr_t server_ip)
{
	src->server_ip = server_ip;
}

void gt_source_set_server_port (GtSource *src, in_port_t server_port)
{
	src->server_port = server_port;
}

void gt_source_set_firewalled (GtSource *src, BOOL fw)
{
	src->firewalled = fw;
}

BOOL gt_source_set_filename (GtSource *src, const char *filename)
{
	char *encoded;

	/* special case for no filename */
	if (!filename)
	{
		free (src->filename);
		src->filename = NULL;
		return TRUE;
	}

	if (!(encoded = gt_url_encode (filename)))
		return FALSE;

	src->filename = encoded;
	return TRUE;
}

BOOL gt_source_set_guid (GtSource *src, const gt_guid_t *guid)
{
	gt_guid_t *dup;

	if (!(dup = gt_guid_dup (guid)))
		return FALSE;

	src->guid = dup;
	return TRUE;
}

/*****************************************************************************/

int gnutella_source_cmp (Protocol *p, Source *a, Source *b)
{
	GtSource *gt_a = NULL;
	GtSource *gt_b = NULL;
	int       ret  = 0;

	if (!(gt_a = gt_source_unserialize (a->url)) ||
	    !(gt_b = gt_source_unserialize (b->url)))
	{
		gt_source_free (gt_a);
		gt_source_free (gt_b);
		return -1;
	}

	if (gt_a->user_ip > gt_b->user_ip)
		ret =  1;
	else if (gt_a->user_ip < gt_b->user_ip)
		ret = -1;

	/*
	 * Having two sources with the same IP on the same transfer can trigger a
	 * bug in most versions of giftd.  At least as of giftd < 0.11.9, this
	 * causes sources to be reactivated after download completion due to a bug
	 * in handle_next_queued called from download_complete.  To avoid that, we
	 * pretend that multiple sources with the same hash from the same user_ip
	 * are the same here, by ignoring the port.  If the sources compare
	 * equally, then one will replace the other when added, and there won't be
	 * any dead sources with the same IP available to reactivate when the
	 * download completes.
	 *
	 * It's ok for the client guid to be different, even if the IPs are the
	 * same, since in that case the guid gets reflected in the user string, so
	 * the bug in handle_next_queue() won't trigger.
	 *
	 * Also, Transfers that don't have hashes are ok since they can only ever
	 * have one user.  So, if either source doesn't have a hash the bug won't
	 * trigger.
	 */
#if 0
	if (gt_a->user_port > gt_b->user_port)
		ret =  1;
	else if (gt_a->user_port < gt_b->user_port)
		ret = -1;
#endif

	/* if both IPs are private match by the guid */
	if (gt_is_local_ip (gt_a->user_ip, gt_a->server_ip) &&
	    gt_is_local_ip (gt_b->user_ip, gt_b->server_ip))
	{
		ret = gt_guid_cmp (gt_a->guid, gt_b->guid);
	}

	if (ret == 0)
	{
		/* if the hashes match consider them equal */
		if (a->hash || b->hash)
			ret = gift_strcmp (a->hash, b->hash);
		else
			ret = gift_strcmp (gt_a->filename, gt_b->filename);
	}

	gt_source_free (gt_a);
	gt_source_free (gt_b);

	return ret;
}

int gnutella_source_add (Protocol *p, Transfer *transfer, Source *source)
{
	GtSource *src;

	assert (source->udata == NULL);

	if (!(src = gt_source_unserialize (source->url)))
		return FALSE;

	source->udata = src;

	/* track this download */
	gt_download_add (transfer, source);

	return TRUE;
}

void gnutella_source_remove (Protocol *p, Transfer *transfer, Source *source)
{
	gt_download_remove (transfer, source);

	assert (source->udata != NULL);
	gt_source_free (source->udata);
}
