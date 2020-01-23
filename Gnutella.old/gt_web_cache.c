/*
 * $Id: gt_web_cache.c,v 1.31 2003/07/14 16:29:56 hipnod Exp $
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

#include "gt_gnutella.h"

#include "lib/platform.h"

#include "file_cache.h"
#include "http_request.h"

#include "gt_connect.h"
#include "gt_node.h"
#include "gt_netorg.h"

#include "gt_web_cache.h"

#include "gt_conf.h"

/*****************************************************************************/

/* minimum time to wait before reconnecting to a webcache */
#define CACHE_RETRY_TIME             (8 * EHOURS)

/*****************************************************************************/

/* number of times we have accessed the gwebcaches */
static int              cache_accesses;

/* the absolute next time we will allow ourselves to access a cache */
static time_t           next_atime;

/* holds all the caches */
static FileCache       *web_caches;

/* proxy server to contact */
static char            *gt_proxy_server;

#if 0
/* webcaches that succeeded connecting, but returned errors or an
 * unparseable response */
static FileCache       *bad_caches;
#endif

/*****************************************************************************/

static void parse_hostfile_response (HttpRequest *http_req, char *hosts_file);
static void parse_urlfile_response  (HttpRequest *http_req, char *url_file);

/*****************************************************************************/

static int parse_web_cache_url (char *value, char **r_host_name,
                                char **r_remote_path)
{
	char  *host_name;

	string_sep (&value, "http://");

	/* divide the url in two parts */
	host_name = string_sep (&value, "/");

	if (r_host_name)
		*r_host_name = host_name;

	if (r_remote_path)
	{
		if (value && *value)
			*r_remote_path = value;
		else
			*r_remote_path = "";
	}

	if (!host_name)
		return FALSE;

	return TRUE;
}

/* parse the extended data in the webcaches file, now its just mtime */
static int parse_web_cache_value (char *value, time_t *r_atime)
{
	time_t atime;

	if ((atime = ATOUL (value)) == (unsigned long) -1)
		atime = 0;

	if (r_atime)
		*r_atime = atime;

	return TRUE;
}

/*****************************************************************************/

#if 0
static Gt_WebCache *gt_web_cache_new (char *host_name, char *remote_path)
{
	Gt_WebCache *web_cache;

	if (!web_caches)
		web_caches = file_cache_new (gift_conf_path ("Gnutella/gwebcaches"));

	if (!(web_cache = malloc (sizeof (Gt_WebCache))))
		return NULL;

	memset (web_cache, 0, sizeof (Gt_WebCache));

	web_cache->remote_path = STRDUP (remote_path);
	web_cache->host_name   = STRDUP (host_name);

	return web_cache;
}

static void gt_web_cache_free (Gt_WebCache *web_cache)
{
	free (web_cache->host_name);
	free (web_cache->remote_path);
	free (web_cache);
}
#endif

/*****************************************************************************/

static void handle_close_request (HttpRequest *req, int error_code)
{
	String *s;

	if (error_code < 200 || error_code >= 300)
	{
		if (error_code == -1)
		{
			/* the error was our fault, out of mem, etc. dont do anything */
			GT->DBGFN (GT, "connect to server %s failed for some reason", 
			           req->host);
		}
		else if (error_code >= 300 && error_code < 400)
		{
			/* redirect: need to handle this at a higher level, actually
			 * this should also remove the cache */
			GT->DBGFN (GT, "%d (redirect?) request: bummer, cant handle yet", 
			           error_code);
		}
		else
		{
			/* not found or internal server error: blacklist the server */
			GT->DBGFN (GT, "server %s returned error %i", req->host, error_code);

			/* well, at least remove the server for now */
			file_cache_remove (web_caches, req->host);
			file_cache_sync (web_caches);
		}
	}

	if ((s = req->data))
		string_free (s);
}

static void parse_hostfile_response (HttpRequest *http_req, char *host_file)
{
	int      hosts = 0;
	GtNode  *node;
	time_t   now;

	if (!host_file)
	{
		GT->DBGFN (GT, "empty host file from %s", http_req->host);
		return;
	}

	GT->DBGFN (GT, "hostfile from server = %s", host_file);

	now = time (NULL);

	while (host_file && *host_file)
	{
		char           *host;
		in_addr_t       ip;
		in_port_t       port;

		host = string_sep_set (&host_file, "\r\n");

		ip   = net_ip (string_sep (&host, ":"));
		port = ATOI   (host);

		if (!port || !ip || ip == (in_addr_t) -1)
			continue;

		GT->DBGFN (GT, "registering %s:%hu (from cache %s)", net_ip_str (ip), 
		           port, http_req->host);

		/* register the hosts as ultrapeers */
		node = gt_node_register (ip, port, GT_NODE_ULTRA, 0, 0);

		/* set the vitality on this node to preserve it across restarts */
		if (node)
			node->vitality = now;

		/* try to connect to the first 5 */
		if (hosts++ < 5 && gt_conn_need_connections (GT_NODE_ULTRA))
			gt_connect (node);
	}
}

static void parse_urlfile_response (HttpRequest *http_req, char *url_file)
{
	int caches = 0;

	if (!url_file)
	{
		GT->DBGFN (GT, "empty url file from %s", http_req->host);
		return;
	}

	GT->DBGFN (GT, "urlfile from server = %s", url_file);

	while (url_file && *url_file)
	{
		char *url;
		char *host_name;
		char *remote_path;

		url = string_sep_set (&url_file, "\r\n");

		/* skip past http:// */
		string_sep (&url, "http://");

		host_name   = string_sep (&url, "/");
		remote_path = url;

		/* NOTE: remote_path is possibly empty */
		if (!host_name)
			continue;

		if (remote_path)
			url = stringf ("http://%s/%s", host_name, remote_path);
		else
			url = stringf ("http://%s/", host_name);

		/* if the webcache is already in our db, skip it */
		if (file_cache_lookup (web_caches, url))
			continue;

		/* 
		 * Only allow caches to register two more caches: this
		 * small number helps to avoid our list of caches getting
		 * polluted.
		 */
		if (++caches > 2)
			break;

		/* format is: <url> <last time visited> */
		file_cache_insert (web_caches, url, "0");
	}

	/* sync the pending web caches to disk */
	file_cache_sync (web_caches);
}

static void end_request (HttpRequest *req, char *data)
{
	char *str = req->request;

	if (str && !strncmp (str, "hostfile", strlen ("hostfile")))
		parse_hostfile_response (req, data);
	else if (str && !strncmp (str, "urlfile", strlen ("urlfile")))
		parse_urlfile_response (req, data);
	else
		GT->DBGFN (GT, "bad request on HttpRequest: %s", req->request);
}

static int handle_recv (HttpRequest *req, char *data, int len)
{
	String *s;

	/* EOF */
	if (!data)
	{
		char *str = NULL;

		if ((s = req->data))
			str = s->str;

		GT->DBGFN (GT, "read %s from server %s", str, req->host);
		end_request (req, str);

		/* clear data link */
		req->data = NULL;

		return TRUE;
	}

	if (!len)
		return TRUE;

	GT->DBGFN (GT, "server sent us: %s", data);

	if (!(s = req->data) && !(s = req->data = string_new (NULL, 0, 0, TRUE)))
		return FALSE;

	if (string_append (s, data) != len)
	{
		GT->DBGFN (GT, "string append failed");
		return FALSE;
	}

	return TRUE;
}

/*****************************************************************************/

static int handle_add_headers (HttpRequest *req, Dataset **headers)
{
	/* without this, HTTP/1.1 uses persistent http */
	dataset_insertstr (headers, "connection", "close");

	return TRUE;
}

/*****************************************************************************/

static int parse_host_and_port (char **r_host, in_port_t *r_port)
{
	char       *str;
	char       *host;
	in_port_t   port;

	str = *r_host;

	if (r_port)
		*r_port = 80;

	/* skip leading 'http://' if found */
	if (strstr (str, "http://"))
		str += strlen ("http://");

	host = string_sep (&str, ":");

	if (!host)
		return FALSE;

	*r_host = host;

	if (str && !string_isempty (str))
	{
		port = ATOI (str);

		/* make sure port is valid */
		if (port <= 0 || port >= 65536)
			return FALSE;

		*r_port = port;
	}

	return TRUE;
}

static TCPC *open_http_connection (char *http_name)
{
	in_addr_t       ip;
	in_port_t       port;
	char           *str;
	char           *name;
	TCPC           *c;
	struct hostent *host;

	if (!http_name)
		return NULL;

	if (!(str = STRDUP (http_name)))
		return NULL;

	name = str;

	if (!parse_host_and_port (&name, &port))
	{
		GT->DBGFN (GT, "error parsing hostname \"%s\"", str);
		free (str);
		return NULL;
	}

	if (!(host = gethostbyname (name)))
	{
#ifndef WIN32
		GT->DBGFN (GT, "lookup failed on \"%s\": %s", name, hstrerror (h_errno));
#endif
		free (str);
		return NULL;
	}

	/* ip is in network-order already */
	memcpy (&ip, host->h_addr, MIN (host->h_length, sizeof (ip)));
	c = tcp_open (ip, port, FALSE);

	if (!c)
	{
		GT->DBGFN (GT, "couldn't open connection to %s [%s]: %s", 
		           http_name, net_ip_str (ip), GIFT_NETERROR());
	}

	free (str);
	return c;
}

/* return the name we have to lookup */
static char *get_http_name (char *name)
{
	char  *proxy;
	char   *host;

	host  = name;
	proxy = HTTP_PROXY;

	string_trim (proxy);

	if (proxy && !string_isempty (proxy))
	{
		/* connect to the proxy instead */
		if (STRCMP (proxy, gt_proxy_server) != 0)
		{
			GT->DBGFN (GT, "using proxy server %s", proxy);
			free (gt_proxy_server);
			gt_proxy_server = STRDUP (proxy);
		}

		host = proxy;
	}

	return host;
}

static int make_request (char *host_name, char *remote_path, char *request)
{
	HttpRequest    *req;
	TCPC           *c;
	char           *resolve_name;

	resolve_name = get_http_name (host_name);

	if (!(c = open_http_connection (resolve_name)))
		return FALSE;

	GT->DBGFN (GT, "opened connection to %s [%s]", 
	           resolve_name, net_ip_str (c->host));

	if (!(req = http_request_new (host_name, remote_path, request)))
	{
		tcp_close (c);
		return FALSE;
	}

	req->recv_func       = (HttpReceiveFunc)   handle_recv;
	req->add_header_func = (HttpAddHeaderFunc) handle_add_headers;
	req->close_req_func  = (HttpCloseFunc)     handle_close_request;

	http_request_set_conn    (req, c);            /* setup references */
	http_request_set_timeout (req, 2 * MINUTES);  /* don't wait forever */
	http_request_set_max_len (req, 65536);        /* don't read forever */

	input_add (c->fd, c, INPUT_WRITE,
	           (InputCallback)http_request_handle, TIMEOUT_DEF);

	return TRUE;
}

/*****************************************************************************/

struct find_rand_args
{
	int    n;
	time_t now;
	char  *url;
	char  *field;
};

/* get a random cache from the webcaches dataset */
static void foreach_rand_cache (ds_data_t *key, ds_data_t *value, 
                                struct find_rand_args *args)
{
	time_t  atime;
	float   range = args->n;
	char   *str;
	char   *hostname, *path;
	int     ret;

	/* decrease probability of selecting the next node */
	args->n++;

	if (!parse_web_cache_value (value->data, &atime))
		return;

	/* skip the cache entirely if we've retried too soon */
	if (args->now - atime < CACHE_RETRY_TIME)
		return;

	/* 
	 * Make sure the cache has a parseable url 
	 *
	 * TODO: This is ugly, it really should be parsed into a 
	 * a data structure once instead.
	 */
	str = STRDUP (key->data);
	ret = parse_web_cache_url (str, &hostname, &path);
	free (str);

	if (!ret)
	{
		GT->warn (GT, "bad webcache url \"%s\" from %s/gwebcaches", 
		          key->data, gift_conf_path ("Gnutella"));
		return;
	}

	/* select this webcache with probability 1/n */
	if (range * rand() / (RAND_MAX + 1.0) < 1.0)
	{
		char *keystr   = key->data;
		char *valuestr = value->data;

		/* free the old values */
		free (args->url);
		free (args->field);

		args->url   = STRDUP (keystr);
		args->field = STRDUP (valuestr);
	}
}

static int get_random_cache (time_t now, char **r_host_name,
                             char **r_remote_path)
{
	int                    ret;
	struct find_rand_args  args;

	args.n     = 1;         /* initial probability */
	args.now   = now;       /* current time */
	args.url   = NULL;
	args.field = NULL;

	dataset_foreach (web_caches->d, DS_FOREACH(foreach_rand_cache), &args);

	if (!args.url)
	{
		GT->DBGFN (GT, "couldn't find random cache");
		return FALSE;
	}

	ret = parse_web_cache_url (args.url, r_host_name, r_remote_path);

	if (!*r_host_name || !*r_remote_path)
	{
		free (args.url);
		free (args.field);
		return FALSE;
	}

	*r_host_name   = STRDUP (*r_host_name);
	*r_remote_path = STRDUP (*r_remote_path);

	/* free the original buffer */
	free (args.url);
	free (args.field);

	return ret;
}

static void access_gwebcaches (void)
{
	int     len;
	char   *host_name;
	char   *remote_path;
	time_t  now;
	int     host_requests = 0;
	int     url_requests  = 0;
	char   *url;
	char   *field;
	int     max_requests = 1;

	GT->DBGFN (GT, "entered");
	now = time (NULL);

	len = dataset_length (web_caches->d);

	if (max_requests > len)
		max_requests = len;

	while (host_requests < max_requests)
	{
		if (!get_random_cache (now, &host_name, &remote_path))
		{
			GT->DBGFN (GT, "error looking up cache");
			break;
		}

		GT->DBGFN (GT, "hitting web cache http://%s/%s",
		           host_name, remote_path);

		/* make a url request sometimes to keep the cache file up to date,
		 * but mostly ask for hosts */
		if (10.0 * rand() / (RAND_MAX + 1.0) < 1.0)
		{
			make_request (host_name, remote_path,
			              "urlfile=1&client=GIFT&version=" GT_VERSION);
			url_requests++;
		}
		else
		{
			make_request (host_name, remote_path,
			              "hostfile=1&client=GIFT&version=" GT_VERSION);
			host_requests++;
		}

		/* reset the atime for the cache */
		url   = stringf_dup ("http://%s/%s", host_name, remote_path);
		field = stringf_dup ("%lu", now);

		file_cache_insert (web_caches, url, field);

		free (url);
		free (field);

		free (host_name);
		free (remote_path);
	}

	file_cache_sync (web_caches);
}

static int webcache_update (void *udata)
{
	char       *webcache_file;
	int         web_exists;
	time_t      now;
	struct stat st;

	if (GNUTELLA_LOCAL_MODE)
		return TRUE;

	now = time (NULL);

	if (now < next_atime) 
		return FALSE;

	webcache_file = STRDUP (gift_conf_path ("Gnutella/gwebcaches"));
	web_exists = file_stat (webcache_file, &st);

	if (!web_exists)
	{
		GIFT_ERROR (("gwebcaches file doesn't exist"));
		return FALSE;
	}

	GT->DBGFN (GT, "accessing gwebcaches, accessed %d previous times",
	           cache_accesses);

	/*
	 * On startup, allow two rapid accesses to the caches in quick
	 * succession. Thereafter, allow one access every 10 minutes, which 
	 * is 12 caches/hour. 
	 *
	 * We won't normally access them even that frequently, though, since
	 * we'll only access them when the number of hosts is low. Since we
	 * only check the same cache once every 8 hours, we need at least 
	 * 8 x 12 = 96 caches in order to always have some caches we haven't
	 * checked available.
	 *
	 * This is important when the link is down for a long time, and we
	 * think we've been talking to caches, and so avoid them. But we 
	 * really haven't, so its good to keep some caches always free.
	 */
	if (cache_accesses < 2)
		next_atime = now + 60 * ESECONDS;
	else
		next_atime = now + 10 * EMINUTES;
		
	access_gwebcaches ();
	cache_accesses++;

	free (webcache_file);
	return TRUE;
}

/*****************************************************************************/

void gt_web_cache_update (void)
{
	int nodes;

	if (gt_nodes_full (&nodes))
		return;

	if (nodes < 20)
		webcache_update (NULL);
}

int gt_web_cache_init (void)
{
	/*
	 * Copy the gwebcaches file to from the data dir to
	 * ~/.giFT/Gnutella if it is newer or if ~/.giFT/Gnutella/gwebcaches
	 * doesn't exist.
	 */
	gt_config_load_file ("Gnutella/gwebcaches", TRUE, FALSE);

	web_caches = file_cache_new (gift_conf_path ("Gnutella/gwebcaches"));

	if (!web_caches)
		return FALSE;

	return TRUE;
}

void gt_web_cache_cleanup (void)
{
	file_cache_free (web_caches);
	web_caches     = NULL;

	cache_accesses = 0;
	next_atime     = 0;
}
