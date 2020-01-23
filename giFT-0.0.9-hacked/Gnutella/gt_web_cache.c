/*
 * $Id: gt_web_cache.c,v 1.22 2003/06/07 07:13:01 hipnod Exp $
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

#include "gt_node.h"
#include "gt_web_cache.h"

#include "gt_conf.h"

/*****************************************************************************/

/* last time webcaches were accessed to lookup hosts */
static time_t           cache_atime;

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
	if (!host_file)
	{
		GT->DBGFN (GT, "empty host file from %s", http_req->host);
		return;
	}

	GT->DBGFN (GT, "hostfile from server = %s", host_file);

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
		gt_node_register (ip, port, GT_NODE_ULTRA, 0, 0);
	}
}

static void parse_urlfile_response (HttpRequest *http_req, char *url_file)
{
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
		 * Hmm, if the user updates the cache here,
		 * it will get reloaded and the new entries will
		 * be lost.
		 */

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

	/* setup circular references */
	c->udata = req;
	req->c = c;

	input_add (c->fd, c, INPUT_WRITE,
	           (InputCallback)handle_http_request, TIMEOUT_DEF);

	return TRUE;
}

/*****************************************************************************/

struct _find_nth_args
{
	int    n;
	char  *url;
	char  *field;
};

static int get_nth (ds_data_t *key, ds_data_t *value,
                    struct _find_nth_args *args)
{
	if (args->n < 0)
		return DS_BREAK;

	if (!args->n--)
	{
		char *keystr   = key->data;
		char *valuestr = value->data;

		args->url   = STRDUP (keystr);
		args->field = STRDUP (valuestr);

		return DS_BREAK;
	}

	return DS_CONTINUE;
}

static int get_random_cache (int length, char **r_host_name,
                             char **r_remote_path, time_t *r_atime)
{
	int     index;
	int     success1, success2;
	struct _find_nth_args args;

	if (!length)
		return FALSE;

	/* get a random index */
	index = (int) (1.0 * length * rand () / (RAND_MAX + 1.0));

	GT->DBGFN (GT, "rand index = %i, nr caches = %i", index, length);

	assert (index < length);

	args.n     = index;
	args.url   = NULL;
	args.field = NULL;

	dataset_foreach_ex (web_caches->d, DS_FOREACH_EX(get_nth), &args);

	if (!args.url || !args.field)
	{
		GT->DBGFN (GT, "coulndt find %ith element in dataset of size %i", 
		           index, length);
		return FALSE;
	}

	success1 = parse_web_cache_url   (args.url, r_host_name, r_remote_path);
	success2 = parse_web_cache_value (args.field, r_atime);

	if (r_host_name)
		*r_host_name = STRDUP (*r_host_name);
	if (r_remote_path)
		*r_remote_path = STRDUP (*r_remote_path);

	/* free the original buffer */
	free (args.url);
	free (args.field);

	if (!success1 || !success2)
		return FALSE;

	return TRUE;
}

static void access_gwebcaches (void)
{
	int     i;
	int     len;
	char   *host_name;
	char   *remote_path;
	time_t  atime;
	time_t  now;
	int     host_requests = 0;
	int     url_requests  = 0;
	char   *url;
	char   *field;
	int     max_tries     = 10;     /* max number of caches to lookup */
	int     max_requests;

	GT->DBGFN (GT, "entered");

	/*
	 * TODO: This is a dumb way to access the caches. Instead, we
	 * should try to contact one, and then if it fails, try another and
	 * keep going until it succeeds, or we just give up. Then we don't have
	 * to put some arbitrary limit when to do accesses, too, and can just
	 * do them on demand.
	 */
	len = dataset_length (web_caches->d);

	now = time (NULL);

	max_requests = MIN (2, len / 8);

	for (i = 0; i < max_tries; i++)
	{
		if (host_requests + url_requests >= max_requests)
			break;

		if (!get_random_cache (len, &host_name, &remote_path, &atime))
		{
			GT->DBGFN (GT, "error looking up cache");
			continue;
		}

		GT->DBGFN (GT, "web cache: %s/%s, last accessed %lu seconds ago", 
		           host_name, remote_path, (atime > 0 ? now - atime : 0));

		/* only access caches for requests around once every two days */
		if (now - atime < 48 * EHOURS)
		{
			free (host_name);
			free (remote_path);
			continue;
		}

		/* make a url request sometimes to keep the cache file up to date,
		 * but mostly ask for hosts */
		if (rand () % 10 == 0)
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

		GT->DBGFN (GT, "hitting cache '%s/%s'", host_name, remote_path);

		/* reset the atime for the cache */
		url   = stringf_dup ("http://%s/%s", host_name, remote_path);
		field = stringf_dup ("%lu", now);

		file_cache_insert (web_caches, url, field);

		free (url);
		free (field);

		free (host_name);
		free (remote_path);
	}
}

static int webcache_update (void *udata)
{
	char       *webcache_file;
	int         web_exists;
	struct stat st;
	time_t      now;

	if (GNUTELLA_LOCAL_MODE)
		return TRUE;

	webcache_file = STRDUP (gift_conf_path ("Gnutella/gwebcaches"));
	web_exists = file_stat (webcache_file, &st);

	now = time (NULL);

	if (!web_exists)
	{
		GIFT_ERROR (("gwebcaches file doesn't exist"));
		return FALSE;
	}

	/*
	 * Once every 20 seconds, check the webcaches.  Note that the
	 * caches are only checked when we have less then 20 hosts in the hosts
	 * cache, so this should only run once or twice.
	 */
	if (now - cache_atime > 20 * ESECONDS)
	{
		cache_atime = now;
		access_gwebcaches ();
	}

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
	web_caches = NULL;
}
