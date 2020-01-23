/*
 * ft_html.c
 *
 * Copyright (C) 2001-2002 giFT project (gift.sourceforge.net)
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
	time_t mtime = 0;

	if (!file_exists (file, NULL, &mtime))
		return;

	if (!strcmp (sec, "nodes"))
		cache_nodes = mtime;
	else if (!strcmp (sec, "shares"))
		cache_shares = mtime;
}

/* tests whether or not the cache needs to be updated
 * NOTE: returns FALSE if the cache is stale */
static int html_cache_check (char *file, char *sec)
{
	time_t mtime = 0;
	int    cmp   = FALSE;

	if (!file_exists (file, NULL, &mtime))
		return FALSE;

	if (!strcmp (sec, "nodes"))
		cmp = (cache_nodes == mtime) ? TRUE : FALSE;
	else if (!strcmp (sec, "shares"))
		cmp = (cache_shares == mtime) ? TRUE : FALSE;

	return cmp;
}

/*****************************************************************************/

static unsigned long total_children = 0;
static unsigned long total_shares   = 0;
static double        total_size     = 0.0; /* GB */

static void build_nodelist (Connection *c, Node *node, FILE *f)
{
	unsigned long shares = 0;
	double        size   = 0.0; /* MB */
	unsigned long avail  = 0;

	assert (node->state == NODE_CONNECTED);

	fprintf (f, "<tr><td>");

	/* add the node link */
	if (!node->firewalled)
	{
		fprintf (f, "<a href='http://%s:%hu/'>",
				 net_ip_str (node->ip), node->http_port);
	}

	fprintf (f, "%s", net_ip_str (node->ip));

	if (!node->firewalled)
		fprintf (f, ":%hu</a>", node->http_port);

	/* get this childs shares to us */
	if (FT_NODE(c)->class & NODE_USER /* NODE_CHILD */)
	{
		ft_shost_digest (node->ip, &shares, &size, &avail);

		if (shares)
		{
			total_children++;
			total_shares += shares;
			total_size += (size / 1024.0);
		}
	}

	fprintf (f,
	         "</td><td>%s</td><td>%s%lu%s</td><td align='right'>"
	         "%s%.02fGB%s</td><td>%s%lu%s</td><td>%s</td></tr>\n",
	         node_class_str (node->class),
	         shares ? "" : "<!--", shares,               shares ? "" : "-->",
	         shares ? "" : "<!--", size / 1024.0,        shares ? "" : "-->",
	         shares ? " &nbsp; " : "<!--", avail, shares ? " &nbsp; " : "-->",
	         (node->firewalled ? "[ firewalled ]" : ""));
}

static Connection *build_nodelist_index (Connection *c, Node *node, FILE *f)
{
	build_nodelist (c, node, f);
	return NULL;
}

static Connection *build_nodelist_search (Connection *c, Node *node, FILE *f)
{
	if (!(node->class & NODE_INDEX))
		build_nodelist (c, node, f);

	return NULL;
}

static Connection *build_nodelist_user (Connection *c, Node *node, FILE *f)
{
	if (!(node->class & NODE_INDEX) && !(node->class & NODE_SEARCH))
		build_nodelist (c, node, f);

	return NULL;
}

/*****************************************************************************/

/* also, unoptimized crap. :) */
static int build_shares (FileShare *file, FILE *f)
{
	char *encoded;
	char *bitrate;
	char *dur, *dur_fmt = NULL;

	if (!file || !SHARE_DATA(file))
		return TRUE;

	assert (SHARE_DATA(file)->hpath != NULL);

	if (!(encoded = url_encode (SHARE_DATA(file)->hpath)))
		return TRUE;

	bitrate = meta_lookup (file, "bitrate");
	dur     = meta_lookup (file, "duration");

	if (dur)
	{
		unsigned long dur_n = ATOUL (dur);
		dur_fmt = stringf ("%lu:%02lu", dur_n / 60, dur_n % 60);
	}

	fprintf (f,
	         "<tr>"
	         " <td>%li</td> "
	         " <td><a href='%s'>%s</a></td> "
	         " <td align='right'>%s</td> "
	         " <td align='right'>%s</td></tr>\n",
	         (long) file->size,
	         encoded, SHARE_DATA(file)->hpath,
	         STRING_NOTNULL (bitrate),
	         STRING_NOTNULL (dur_fmt));

	free (encoded);

	return TRUE;
}

static void dump_header (FILE *f)
{
	fprintf (f,
	         "<!DOCTYPE html PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\" \"http://www.w3.org/TR/html4/loose.dtd\">\n"
	         "<html>\n"
	         " <head>\n"
	         "  <title>giFT: OpenFT</title>\n"
	         "  <link rel='stylesheet' type='text/css' href='" IMAGE_HOST "/other.css'>\n"
	         "  <meta name='robots' content='noindex,nofollow,noarchive'>\n"
	         " </head>\n"
	         " <body bgcolor='#788896'>\n"
	         "  <table border='0' cellspacing='0' cellpadding='0' width='793' align='center'>\n"
	         "   <tr>\n"
	         "    <td><img src='" IMAGE_HOST "/spacer.gif' width='3' height='1' alt='' border='0'></td>\n"
	         "    <td><img src='" IMAGE_HOST "/spacer.gif' width='703' height='1' alt='' border='0'></td>\n"
	         "    <td><img src='" IMAGE_HOST "/spacer.gif' width='3' height='1' alt='' border='0'></td>\n"
	         "    <td><img src='" IMAGE_HOST "/spacer.gif' width='84' height='1' alt='' border='0'></td>\n"
	         "   </tr>\n"
	         "   <tr>\n"
	         "    <td colspan='4'><a href='http://www.giftproject.org/'>"
	         "<img src='" IMAGE_HOST "/top.jpg' alt='giFT: Internet File Transfer' border='0'>"
	         "</a></td>\n"
	         "   </tr>\n"
	         "   <tr>\n"
	         "    <td bgcolor='#000000'>"
	         "<img src='" IMAGE_HOST "/spacer.gif' width='3' height='1' alt='' border='0'></td>\n"
	         "    <td bgcolor='#ffffff'>\n"
	         "     <table border='0' cellpadding='5' width='100%%'>\n"
	         "      <tr><td>\n"
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
	int max_uploads;

	fprintf (f, "<h1>Node Status: %s</h1>\n", node_class_str (FT_SELF->class));

	fprintf (f, "<p>Active Uploads: %i", upload_length (NULL));

	if ((max_uploads = upload_status ()) != -1)
		fprintf (f, " / %i", max_uploads);

	fprintf (f, "</p>\n");
}

static void dump_body_nodes (FILE *f, int link_only)
{
	fprintf (f,
			 "<!-- begin body_nodes -->\n"
	         "<h2>Node List</h2>\n"
	         "<table border='0' cellspacing='1' cellpadding='2'>\n"
	         "<tr>\n"
	         " <th>Node</th>\n"
	         " <th>Classification</th>\n"
	         " <th>Shares</th>\n"
	         " <th>Size</th>\n"
	         " <th><!-- padding --></th>\n"
	         " <th><!-- firewalled --></th>\n"
	         "</tr>\n");

	/* insert nodelist here */
	if (link_only)
	{
		fprintf (f,
		         "<tr><td colspan='6'>\n"
		         " <a href='/nodes.html'>Show Nodes...</a>\n"
		         "</td></tr>\n");
	}
	else
	{
		conn_foreach ((ConnForeachFunc) build_nodelist_index, f,
					  NODE_INDEX, NODE_CONNECTED, 0);
		conn_foreach ((ConnForeachFunc) build_nodelist_search, f,
					  NODE_SEARCH, NODE_CONNECTED, 0);
		conn_foreach ((ConnForeachFunc) build_nodelist_user, f,
					  NODE_USER, NODE_CONNECTED, 0);
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
	         "<table border='0' cellspacing='3' cellpadding='2'>\n"
	         "<tr>\n"
	         " <th>Filesize</th>\n"
	         " <th>Filename</th>\n"
			 " <th>Bitrate</th>\n"
			 " <th>Duration</th>\n"
	         "</tr>\n");

	/* insert shares list here */
	if (link_only)
	{
		fprintf (f,
		         "<tr><td colspan='2'>\n"
		         " <a href='/shares.html'>Show Shares...</a>\n"
		         "</td></tr>\n");
	}
	else
	{
		list_foreach_remove (share_index_sorted (),
		                     (ListForeachFunc) build_shares, f);
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
	         "<br><br>\n"
	         "<p align='right'>\n"
	         " <a href='http://sourceforge.net/projects/gift'>"
	         "<img src='http://sourceforge.net/sflogo.php?group_id=34618' border='0' alt='SourceForge Logo'></a>\n"
	         "</p>\n"
	         "<!-- END FOOTER -->\n"
	         "     </td></tr>\n"
	         "    </table>\n"
	         "   </td>\n"
	         "   <td bgcolor='#000000'>"
	         "<img src='" IMAGE_HOST "/spacer.gif' width='3' height='1' alt='' border='0'></td>\n"
	         "   <td background='" IMAGE_HOST "/bg.gif' valign='top' rowspan='2'>"
	         "<img src='" IMAGE_HOST "/upperrightcorner.jpg' alt='' border='0'></td>\n"
	         "  </tr>\n"
	         "  <tr>\n"
	         "   <td bgcolor='#000000' colspan='3'>"
	         "<img src='" IMAGE_HOST "/spacer.gif' width='3' height='3' alt='' border='0'></td>\n"
	         "  </tr>\n"
	         " </table>\n"
	         " </body>\n"
	         "</html>\n");
}

char *html_page_index (in_addr_t ip, char *file, char *sec)
{
	FILE *f;
	int   nodes_link  = FALSE;
	int   shares_link = FALSE;

	TRACE (("%s", net_ip_str (ip)));

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
