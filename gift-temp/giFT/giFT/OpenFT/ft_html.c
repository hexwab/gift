/*
 * $Id: ft_html.c,v 1.20 2003/05/05 09:49:09 jasta Exp $
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

#include <sys/types.h>
#include <sys/stat.h>
#include <ctype.h>

#include "ft_openft.h"

#include "ft_netorg.h"

#include "ft_xfer.h"
#include "ft_html.h"
#include "file.h"
#include "meta.h"

#include "parse.h"

/*****************************************************************************/

#define PRIVATE_NODEPAGE config_get_int (OPENFT->conf, "nodepage/private=0")
#define PRIVATE_ALLOW    config_get_str (OPENFT->conf, "local/hosts_allow=LOCAL")

/*****************************************************************************/

#define IMAGE_HOST "/OpenFT"

static time_t cache_nodes  = 0;
static time_t cache_shares = 0;

/*****************************************************************************/

/* strip HTML */
char *html_strip (char *html)
{
	char *head, *stripped, *begin, *end;

	if (!html)
		return NULL;

	/* not very effecient, but clean */
	head = stripped = strdup (html);

	while ((begin = strchr (head, '<')))
	{
		/* a '<' was found, look for the '>' */
		for (end = begin + 1; *end && *end != '>'; end++);

		if (*end)
			end++;

		/* shift the string back */
		string_move (begin, end);

		head = begin;
	}

	return stripped;
}

/*****************************************************************************/
/* url decode/encode helpers */

static int oct_value_from_hex (char hex_char)
{
	if (!isxdigit (hex_char))
		return 0;

	if (hex_char >= '0' && hex_char <= '9')
		return (hex_char - '0');

	hex_char = toupper (hex_char);

	return ((hex_char - 'A') + 10);
}

char *url_decode (char *encoded)
{
	char *decoded, *ptr;

	if (!encoded)
		return NULL;

	/* make sure we are using our own memory here ... */
	ptr = strdup (encoded);

	/* save the head */
	decoded = ptr;

	/* convert '+' -> ' ' and %2x -> char value */
	while (*ptr)
	{
		switch (*ptr)
		{
		 case '+':
			*ptr = ' ';
			break;
		 case '%':
			if (isxdigit (ptr[1]) && isxdigit (ptr[2]))
			{
				int oct_val;

				oct_val =  oct_value_from_hex (ptr[1]) * 16;
				oct_val += oct_value_from_hex (ptr[2]);

				*ptr = (char) oct_val;

				string_move (ptr + 1, ptr + 3);
			}
			break;
		 default:
			break;
		}

		ptr++;
	}

	return decoded;
}

static char *url_encode_char (char *stream, unsigned char c)
{
	*stream++ = '%';

	sprintf (stream, "%02x", (unsigned int) c);

	return stream + 2;
}

char *url_encode (char *decoded)
{
	char *encoded, *ptr;

	if (!decoded)
		return NULL;

	/* allocate a large enough buffer for all cases */
	encoded = ptr = malloc ((strlen (decoded) * 3) + 1);

	while (*decoded)
	{
		/* we can rule out non-printable and whitespace characters */
		if (!isprint (*decoded) || isspace (*decoded))
			ptr = url_encode_char (ptr, *decoded);
		else
		{
			/* check for anything special */
			switch (*decoded)
			{
			 case '?':
			 case '#':
			 case '@':
			 case '+':
			 case '%':
			 case '&':
			 case ':':
			 case '=':
			 case '(':
			 case ')':
			 case '[':
			 case ']':
			 case '\"':
			 case '\'':
				ptr = url_encode_char (ptr, *decoded);
				break;
			 default: /* regular character, just copy */
				*ptr++ = *decoded;
				break;
			}
		}

		decoded++;
	}

	*ptr = 0;

	return encoded;
}

/*****************************************************************************/

void html_cache_flush (char *sec)
{
	if (!sec || !strcmp (sec, "nodes"))
		cache_nodes = time (NULL);

	if (!sec || !strcmp (sec, "shares"))
		cache_shares = cache_nodes;
}

static void html_cache_set (char *file, char *sec)
{
	struct stat st;

	if (!file_stat (file, &st))
		return;

	if (!strcmp (sec, "nodes"))
		cache_nodes = st.st_mtime;
	else if (!strcmp (sec, "shares"))
		cache_shares = st.st_mtime;
}

/* tests whether or not the cache needs to be updated
 * NOTE: returns FALSE if the cache is stale */
static int html_cache_check (char *file, char *sec)
{
	struct stat st;
	int         cmp = FALSE;

	if (!file_stat (file, &st))
		return FALSE;

	if (!strcmp (sec, "nodes"))
		cmp = (cache_nodes == st.st_mtime) ? TRUE : FALSE;
	else if (!strcmp (sec, "shares"))
		cmp = (cache_shares == st.st_mtime) ? TRUE : FALSE;

	return cmp;
}

/*****************************************************************************/

static unsigned long total_children = 0;
static unsigned long total_shares   = 0;
static double        total_size     = 0.0; /* GB */

static void build_nodelist (FTNode *node, FILE *f)
{
	unsigned long shares = 0;
	double        size   = 0.0; /* MB */
	unsigned long avail  = 0;

	assert (node->state == NODE_CONNECTED);

	fprintf (f, "<tr><td>");

	/* add the node link */
	if (!ft_node_fw (node))
	{
		fprintf (f, "<a href='http://%s:%hu/'>",
				 net_ip_str (node->ip), node->http_port);
	}

	fprintf (f, "%s", net_ip_str (node->ip));

	if (!ft_node_fw (node))
		fprintf (f, ":%hu</a>", node->http_port);

	fprintf (f, "</td>\n");

	/* get this childs shares to us */
	if (node->klass & NODE_USER /* NODE_CHILD */)
	{
		ft_shost_digest (node->ip, &shares, &size, &avail);

		if (shares)
		{
			total_children++;
			total_shares += shares;
			total_size += (size / 1024.0);
		}
	}

	fprintf (f, "<td><em>%s</em></td>", STRING_NOTNULL(node->alias));
	fprintf (f, "<td>%s</td>", ft_node_classstr_full (node->klass));
	fprintf (f, "<td>%s</td>",
	         shares ? stringf ("%lu", shares) : "");
	fprintf (f, "<td align='right'>%s</td>",
	         shares ? stringf ("%.02fGB", size / 1024.0) : "");
	fprintf (f, "<td>%s</td>",
	         shares ? stringf ("%lu", avail) : "");
	fprintf (f, "</tr>\n");
}

static int build_nodelist_index (FTNode *node, FILE *f)
{
	build_nodelist (node, f);
	return TRUE;
}

static int build_nodelist_search (FTNode *node, FILE *f)
{
	if (node->klass & NODE_INDEX)
		return FALSE;

	build_nodelist (node, f);
	return TRUE;
}

static int build_nodelist_user (FTNode *node, FILE *f)
{
	if ((node->klass & NODE_INDEX) || (node->klass & NODE_SEARCH))
		return FALSE;

	build_nodelist (node, f);
	return TRUE;
}

/*****************************************************************************/

/* also, unoptimized crap. :) */
static int build_shares (FileShare *file, FILE *f)
{
	char *encoded;
	char *bitrate, *bitrate_fmt = NULL;
	char *dur, *dur_fmt = NULL;

	if (!file || !SHARE_DATA(file))
		return TRUE;

	/* assert (SHARE_DATA(file)->hpath != NULL); */

	if (!(encoded = url_encode (SHARE_DATA(file)->hpath)))
		return TRUE;

	if ((bitrate = meta_lookup (file, "bitrate")))
	{
		unsigned long bitrate_n = ATOUL (bitrate);
		bitrate_fmt = stringf_dup ("%lukbps", (bitrate_n / 1000));
	}

	if ((dur = meta_lookup (file, "duration")))
	{
		unsigned long dur_n = ATOUL (dur);
		dur_fmt = stringf_dup ("%lu:%02lu", dur_n / 60, dur_n % 60);
	}

	fprintf (f,
	         "<tr>"
	         " <td>%lu</td> "
	         " <td><a href='%s'>%s</a></td> "
	         " <td align='right'>%s</td> "
	         " <td align='right'>%s</td></tr>\n",
	         (unsigned long)file->size,
	         encoded, SHARE_DATA(file)->hpath,
	         STRING_NOTNULL(bitrate_fmt),
	         STRING_NOTNULL(dur_fmt));

	free (bitrate_fmt);
	free (dur_fmt);
	free (encoded);

	return TRUE;
}

static void dump_header (FILE *f)
{
	fprintf (f,
	         "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">\n"
	         "<html>\n"
	         " <head>\n"
	         "  <title>OpenFT nodepage</title>\n"
	         "  <meta name='robots' content='noindex,nofollow,noarchive'>\n"
	         " </head>\n"
	         " <body>\n"
	         "<!-- begin openft body -->\n");

	/* separated for better tracking of the %s */
	fprintf (f,
	         "<h1><a href='http://www.giftproject.org/'>%s %s</a> "
	         "(OpenFT revision: %hu.%hu.%hu-%hu)</h1>\n",
	         PACKAGE, VERSION, OPENFT_MAJOR, OPENFT_MINOR, OPENFT_MICRO,
	         OPENFT_REV);
}

static void dump_header_body (FILE *f)
{
	fprintf (f,
	         "<h2>Node Alias: %s</h2>\n"
	         "<h2>Node Status: %s</h2>\n",
	         STRING_NOTNULL(FT_SELF->alias),
	         ft_node_classstr_full (FT_SELF->klass));
	fprintf (f,
	         "<h3>Statistics</h3>\n"
	         "<table border='1' cellspacing='0' cellpadding='3'>\n");
	fprintf (f,
			 "  <tr><td>Active Uploads</td><td>%i (max_uploads = %i)</td></tr>\n",
	         upload_length (NULL), upload_status ());
	fprintf (f,
	         "</table>\n");
}

static void dump_body_nodes (FILE *f, int link_only)
{
	fprintf (f,
			 "<!-- begin body_nodes -->\n"
	         "<h3>Connections</h3>\n"
	         "<table border='1' cellspacing='0' cellpadding='3'>\n");

	/* reset */
	total_children = 0;
	total_shares   = 0;
	total_size     = 0.0;              /* GB */

	/* insert nodelist here */
	if (link_only)
	{
		fprintf (f,
		         "<tr><td>\n"
				 " <a href='/nodes.html'>Show Nodes...</a>\n"
				 "</td></tr>\n");
	}
	else
	{
		ft_netorg_foreach (NODE_INDEX, NODE_CONNECTED, 0,
		                   FT_NETORG_FOREACH(build_nodelist_index), f);
		ft_netorg_foreach (NODE_SEARCH, NODE_CONNECTED, 0,
		                   FT_NETORG_FOREACH(build_nodelist_search), f);
		ft_netorg_foreach (NODE_USER, NODE_CONNECTED, 0,
		                   FT_NETORG_FOREACH(build_nodelist_user), f);
	}

	fprintf (f,
			 "</table>\n"
			 "<!-- end body_nodes (%lu, %lu, %.02fGB) -->\n",
			 total_children, total_shares, total_size);
}

static void dump_body_shares (FILE *f, int link_only)
{
	fprintf (f,
			 "<!-- begin body_shares -->\n"
	         "<h2>Shares</h2>\n"
	         "<table border='1' cellspacing='0' cellpadding='3'>\n");

	/* insert shares list here */
	if (link_only)
	{
		fprintf (f,
		         "<tr><td>\n"
		         " <a href='/shares.html'>Show Shares...</a>\n"
		         "</td></tr>\n");
	}
	else
	{
		list_foreach_remove (share_index_sorted (),
		                     (ListForeachFunc)build_shares, f);
	}

	fprintf (f,
			 "</table>\n"
			 "<!-- end body_shares -->\n");
}

static void dump_footer (FILE *f)
{
	fprintf (f,
	         "<!-- end openft body -->\n"
	         "<!-- begin footer -->\n"
	         " </body>\n"
	         "</html>\n");
}

char *html_page_index (in_addr_t ip, char *file, char *sec)
{
	FILE *f;
	int   nodes_link  = FALSE;
	int   shares_link = FALSE;

	FT->DBGFN (FT, "%s", net_ip_str (ip));

	if (!file)
		return NULL;

	/* TODO -- 'sorry, you do not have access to this nodepage' message */
	if (PRIVATE_NODEPAGE)
	{
		if (!net_match_host (ip, PRIVATE_ALLOW))
			return NULL;
	}

	if (html_cache_check (file, sec))
		return file;

	/* can't update, so just deliver what we have */
	if (!(f = fopen (file, "w")))
		return file;

	dump_header (f);
	dump_header_body (f);

	/* figure out how to display the actual body based on section supplied */
	if (!strcmp (sec, "nodes"))
		shares_link = TRUE;
	else if (!strcmp (sec, "shares"))
		nodes_link = TRUE;

	dump_body_nodes (f, nodes_link);
	dump_body_shares (f, shares_link);

	dump_footer (f);

	fclose (f);

	/* modify the global mtime */
	html_cache_set (file, sec);

	return file;
}
