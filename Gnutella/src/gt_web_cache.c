/*
 * $Id: gt_web_cache.c,v 1.65 2006/08/06 16:53:36 hexwab Exp $
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

#include "file_cache.h"
#include "http_request.h"

#include "gt_connect.h"
#include "gt_node.h"
#include "gt_node_list.h"
#include "gt_netorg.h"

#include "gt_web_cache.h"
#include "gt_conf.h"

#include "dns.h"

/*****************************************************************************/

/* minimum time to wait before reconnecting to a webcache */
#define CACHE_RETRY_TIME             (8 * EHOURS)

/*****************************************************************************/

/* number of times we have hit some gwebcaches */
static int              cache_hits;

/* the absolute next time we will allow ourselves to access a cache */
static time_t           next_atime;

/* amount of time to layoff the caches once we've received some data */
static time_t           backoff_time    = 1 * EHOURS;

/* holds all the caches */
static FileCache       *web_caches;

/* proxy server to contact */
static char            *gt_proxy_server;

/* webcaches that succeeded connecting, but returned errors or an
 * unparseable response */
static FileCache       *bad_caches;

/* whether we are in the process of checking the caches */
static BOOL             checking_caches;

/*****************************************************************************/

static void parse_hostfile_response (HttpRequest *http_req, char *hosts_file);
static void parse_urlfile_response  (HttpRequest *http_req, char *url_file);

/*****************************************************************************/

/* parse the extended data in the webcaches file, now its just mtime */
static BOOL parse_web_cache_value (char *value, time_t *r_atime)
{
	time_t atime;

	if ((atime = ATOUL (value)) == (unsigned long) -1)
		atime = 0;

	if (r_atime)
		*r_atime = atime;

	return TRUE;
}

/*****************************************************************************/

static char *new_webcache_url (const char *host, const char *path)
{
	return stringf_dup ("http://%s/%s", host, STRING_NOTNULL(path));
}

static void ban_webcache (HttpRequest *req, const char *why)
{
	char *url;

	url = new_webcache_url (req->host, req->path);
	GT->dbg (GT, "banning webcache %s", url);

	file_cache_insert (bad_caches, url, why);
	file_cache_sync (bad_caches);

	free (url);
}

static void insert_webcache (const char *host_name, const char *remote_path,
                             time_t atime)
{
	char *url;
	char *field;

	url   = new_webcache_url (host_name, remote_path);
	field = stringf_dup ("%lu", atime);

	file_cache_insert (web_caches, url, field);

	free (url);
	free (field);
}

/*****************************************************************************/

static void handle_close_request (HttpRequest *req, int error_code)
{
	String *s;

	if (error_code < 0 || error_code < 200 || error_code >= 300)
	{
		if (error_code == -1)
		{
			/* the error was our fault, out of mem, etc. dont do anything */
			GT->DBGFN (GT, "connect to server %s failed for some reason",
			           req->host);
		}
		else
		{
			char err[32];

			snprintf (err, sizeof(err), "Received error %d", error_code);

			/*
			 * Not found, internal server error, or too many redirects: ban
			 * the server's URL
			 */
			GT->DBGFN (GT, "server %s returned error %i", req->host,
			           error_code);
			ban_webcache (req, err);
		}
	}

	/* TODO: this assumes this is the one hostfile request flying around,
	 * and not a urlfile request, which probably needs to be handled
	 * separately */
	checking_caches = FALSE;

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

	/*
	 * If the response start with "ERROR: " (or pseudo-html '<' char), ban the
	 * webcache.
	 */
	if (!strncasecmp (host_file, "ERROR", sizeof ("ERROR") - 1) ||
	    host_file[0] == '<')
	{
		ban_webcache (http_req, "Malformed response content");
		return;
	}

	while (host_file && *host_file)
	{
		char           *host;
		in_addr_t       ip;
		in_port_t       port;

		host = string_sep_set (&host_file, "\r\n");

		ip   = net_ip (string_sep (&host, ":"));
		port = ATOI   (host);

		if (!port || !ip || ip == INADDR_NONE)
			continue;

		GT->DBGFN (GT, "registering %s:%hu (from cache %s)", net_ip_str (ip),
		           port, http_req->host);

		/* register the hosts as ultrapeers */
		node = gt_node_register (ip, port, GT_NODE_ULTRA);
		hosts++;

		if (!node)
			continue;

		/* set the vitality on this node to preserve it across restarts */
		node->vitality = now;

		/* might be connected already */
		if (node->state != GT_NODE_DISCONNECTED)
			continue;

		/* try to connect to the first 5 */
		if (hosts <= 5 && gt_conn_need_connections (GT_NODE_ULTRA))
			gt_connect (node);

		/* don't allow the cache to register an infinite number of hosts */
		if (hosts >= 50)
			break;
	}

	/* save the nodes we added to disk so we dont hit the caches again */
	gt_node_list_save ();

	/*
	 * Do an exponential backoff from the caches. If we were online and
	 * able to receive data, we should be getting node information
	 * some other way now.
	 */
	if (hosts >= 5)
	{
		next_atime    = now + backoff_time;
		backoff_time *= 2;
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

		url = stringf ("http://%s/%s", host_name, STRING_NOTNULL(remote_path));

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
		abort ();
}

/*****************************************************************************/

/*
 * Return TRUE if newname is in the same domain as oldname.  For example,
 * "new.gwc.example.com", "example.com", and "cache.example.com" are all
 * considered in the same domain as "www.example.com".
 *
 * This is called on redirects, to make sure the cache can't redirect to an
 * innocent site as part of a DDoS attack.
 */
static BOOL in_same_domain (const char *oldname, const char *newname)
{
	return FALSE;
#if 0
	const char *p;
	const char *largest = NULL;
	int         periods = 0;

	p = newname;

	/* get the largest common substring */
	while (p != NULL)
	{
		if ((largest = strstr (oldname, p)))
			break;

		/* advance to next domain part */
		p = strchr (p + 1, '.');
	}

	if (!largest)
		return FALSE;

	/*
	 * Make sure the substring matches completely to the end.  This will
	 * actually fail when it shouldn't if one name includes the '.' toplevel
	 * domain and one doesn't.  Oh well.
	 */
	if (strcmp (largest, p) != 0)
		return FALSE;

	/*
	 * Count the number of periods to find the number of subdomains in the
	 * largest common substring.
	 */
	for (p = largest; *p != 0; p++)
	{
		if (*p == '.')
			periods++;
	}

	/*
	 * If the last character is the root '.', subtract one, since we are
	 * looking for the number of common subdomains, and the root is shared by
	 * all names.
	 */
	if (largest[strlen (largest) - 1] == '.')
		periods--;

	/*
	 * If there are two periods, at least two toplevel domains match.
	 */
	if (periods >= 2)
		return TRUE;

	/*
	 * If there is only one period shared, the names MAY be in the same
	 * domain: one of the names has to be completely contained within the
	 * other, such as the case of "foo.example.com" and "example.com".
	 */
	if (periods == 1 &&
	    (strcmp (largest, oldname) == 0 || strcmp (largest, newname) == 0))
	{
		return TRUE;
	}

	/* not in same domain */
	return FALSE;
#endif
}

/*
 * Called to when the webcache sends a 300-level response with a provided
 * Location: header.  Have to make sure the domain the cache directs us
 * to is the same.
 */
static BOOL handle_redirect (HttpRequest *req, const char *new_host,
                             const char *new_path)
{
	assert (new_host != NULL);

	if (in_same_domain (req->host, new_host) == FALSE)
		return FALSE;

	/* might want to do something else if the ban list later becomes per host
	 * rather than per URL */
	ban_webcache (req, "Redirected");

	GT->DBGFN (GT, "Redirecting to new webcache %s/%s", new_host, new_path);

	insert_webcache (new_host, new_path, time (NULL));
	file_cache_sync (web_caches);

	return TRUE;
}

/*****************************************************************************/

static BOOL handle_recv (HttpRequest *req, char *data, size_t len)
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

static BOOL handle_add_headers (HttpRequest *req, Dataset **headers)
{
	/* don't let intermediaries cache our request, I think */
	dataset_insertstr (headers, "Cache-Control", "no-cache");

	return TRUE;
}

/*****************************************************************************/

static BOOL parse_host_and_port (char **r_host, in_port_t *r_port)
{
	char  *str;
	char  *host;
	long   port;

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
		port = gift_strtol (str);

		/* make sure port is valid */
		if (port <= 0 || port >= 65536)
			return FALSE;

		*r_port = port;
	}

	return TRUE;
}

static TCPC *open_http_connection (HttpRequest *req, const char *http_name)
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

	if (!(host = gt_dns_lookup (name)))
	{
		free (str);
		return NULL;
	}

	/* ip is in network-order already */
	memcpy (&ip, host->h_addr, MIN (host->h_length, sizeof (ip)));

	if (net_match_host (ip, "LOCAL"))
	{
		free (str);
		ban_webcache (req, "Resolved to local IP");
		return NULL;
	}

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
	char  *host;

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

static void check_dns_error (const char *name, HttpRequest *req)
{
	int error;

	error = gt_dns_get_errno ();

	if (!error)
		return;

	GT->DBGFN (GT, "lookup failed on \"%s\": %s", name, gt_dns_strerror(error));

	/* ban the host, but only if not using a proxy server */
	if (error == HOST_NOT_FOUND && gt_proxy_server == NULL)
	{
		GT->DBGFN (GT, "webcache \"%s\" not in DNS. banning", name);
		ban_webcache (req, "Host not found in DNS");
		return;
	}
}

static BOOL make_request (char *host_name, char *remote_path, char *request)
{
	HttpRequest    *req;
	TCPC           *c;
	char           *resolve_name;
	char           *url;

	url = stringf_dup ("http://%s/%s", host_name, STRING_NOTNULL(remote_path));

	if (!(req = gt_http_request_new (url, request)))
	{
		free (url);
		return FALSE;
	}

	free (url);

	resolve_name = get_http_name (host_name);

	gt_dns_set_errno (0);

	if (!(c = open_http_connection (req, resolve_name)))
	{
		check_dns_error (resolve_name, req);
		gt_http_request_close (req, -1);
		return FALSE;
	}

	GT->DBGFN (GT, "opening connection to %s [%s]",
	           resolve_name, net_ip_str (c->host));

	req->recv_func       = handle_recv;
	req->add_header_func = handle_add_headers;
	req->close_req_func  = handle_close_request;
	req->redirect_func   = handle_redirect;

	gt_http_request_set_conn    (req, c);               /* setup references */
	gt_http_request_set_proxy   (req, gt_proxy_server); /* maybe use proxy */
	gt_http_request_set_timeout (req, 2 * MINUTES);     /* don't wait forever */
	gt_http_request_set_max_len (req, 65536);           /* don't read forever */

	input_add (c->fd, c, INPUT_WRITE,
	           (InputCallback)gt_http_request_handle, TIMEOUT_DEF);

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
	char   *url   = key->data;
	char   *hostname, *path;
	int     ret;

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
	str = STRDUP (url);
	ret = gt_http_url_parse (str, &hostname, &path);
	free (str);

	if (!ret)
	{
		GT->warn (GT, "bad webcache url \"%s\" from %s/gwebcaches",
		          key->data, gift_conf_path ("Gnutella"));
		return;
	}

	/* decrease probability of selecting the next web cache */
	args->n++;

	/*
	 * Select this webcache with probability 1/n.
	 *
	 * Also select this cache if we haven't chosen one yet, which may be the
	 * case on if the index of the cache is > 0 when there are banned caches.
	 */
	if (args->url == NULL ||
	    range * rand() / (RAND_MAX + 1.0) < 1.0)
	{
		char *keystr   = key->data;
		char *valuestr = value->data;

		/* check if this is a bad gwebcache */
		if (file_cache_lookup (bad_caches, url))
		{
#if 1
			GT->warn (GT, "skipping webcache %s, in bad gwebcaches", url);
#endif
			/* pretend we didn't select this to ensure equal distribution */
			args->n--;

			return;
		}

		/* free the old values */
		free (args->url);
		free (args->field);

		args->url   = STRDUP (keystr);
		args->field = STRDUP (valuestr);
	}
}

static BOOL get_random_cache (time_t now, char **r_host_name,
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

	ret = gt_http_url_parse (args.url, r_host_name, r_remote_path);

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
#if 0
	int     url_requests  = 0;
#endif
	int     max_requests = 1;
	BOOL    ret;
	BOOL    need_sync;

	/*
	 * We may get called while a check of the gwebcaches is already
	 * in progress.
	 */
	if (checking_caches)
	{
		GT->DBGFN (GT, "Access already in progress");
		return;
	}

	now = time (NULL);

	len = dataset_length (web_caches->d);

	if (max_requests > len)
		max_requests = len;

	need_sync = FALSE;

	while (host_requests < max_requests)
	{
		if (!get_random_cache (now, &host_name, &remote_path))
		{
			GT->DBGFN (GT, "error looking up cache");
			break;
		}

#if 0
		/* make a url request sometimes to keep the cache file up to date, but
		 * mostly ask for hosts */
		if (10.0 * rand() / (RAND_MAX + 1.0) < 1.0)
		{
			ret = make_request (host_name, remote_path,
			                    "urlfile=1&client=GIFT&version=" GT_VERSION);
			url_requests++;
		}
		else
#endif
		{
			ret = make_request (host_name, remote_path,
			                    "hostfile=1&client=GIFT&version=" GT_VERSION);

			if (ret)
				checking_caches = TRUE;

			host_requests++;
		}

		if (ret)
		{
			GT->DBGFN (GT, "hitting web cache [total cache hits %u] "
			           "(cache: http://%s/%s)", cache_hits,
			           host_name, STRING_NOTNULL(remote_path));

			cache_hits++;
			need_sync = TRUE;

			/* reset the atime for the cache */
			insert_webcache (host_name, remote_path, now);
		}

		free (host_name);
		free (remote_path);
	}

	/* only sync when we successfully accessed a cache */
	if (need_sync)
		file_cache_sync (web_caches);
}

static BOOL webcache_update (void *udata)
{
	char       *webcache_file;
	int         web_exists;
	time_t      now;
	size_t      nodes_len;
	struct stat st;

	if (GNUTELLA_LOCAL_MODE)
		return TRUE;

	now = time (NULL);
	nodes_len = gt_conn_length (GT_NODE_NONE, GT_NODE_ANY);

	/*
	 * If we've already accessed the caches successfully, we won't
	 * allow another access to go through, _unless_ the node list
	 * is small enough, in which case it could be we really do need
	 * to access the caches.
	 */
	if (now < next_atime && nodes_len >= 20)
		return FALSE;

	webcache_file = STRDUP (gift_conf_path ("Gnutella/gwebcaches"));
	web_exists = file_stat (webcache_file, &st);

	if (!web_exists)
	{
		GIFT_ERROR (("gwebcaches file doesn't exist"));
		return FALSE;
	}

	/*
	 * next_atime, the absolute next time we allow ourselves to contact the
	 * caches, gets set when we sucessfully access the caches, and if we
	 * manage to get some hosts from a cache we access in an exponentially
	 * decreasing interval.
	 */
	access_gwebcaches ();

	free (webcache_file);
	return TRUE;
}

/*****************************************************************************/

void gt_web_cache_update (void)
{
	webcache_update (NULL);
}

BOOL gt_web_cache_init (void)
{
	/*
	 * Copy the gwebcaches file to from the data dir to
	 * ~/.giFT/Gnutella if it is newer or if ~/.giFT/Gnutella/gwebcaches
	 * doesn't exist.
	 */
	gt_config_load_file ("Gnutella/gwebcaches", TRUE, FALSE);

	web_caches = file_cache_new (gift_conf_path ("Gnutella/gwebcaches"));
	bad_caches = file_cache_new (gift_conf_path ("Gnutella/bad_gwebcaches"));

	if (!web_caches)
		return FALSE;

	return TRUE;
}

void gt_web_cache_cleanup (void)
{
	file_cache_free (web_caches);
	web_caches = NULL;

	file_cache_free (bad_caches);
	bad_caches = NULL;

	cache_hits = 0;
	next_atime = 0;

	checking_caches = FALSE;
}
