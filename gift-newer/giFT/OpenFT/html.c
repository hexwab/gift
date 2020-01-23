/*
 * html.c
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

#include "openft.h"

#include "netorg.h"

#include "http.h"
#include "html.h"

#include "parse.h"

/*****************************************************************************/

#define IMAGE_HOST "/OpenFT"

static time_t nodepage_mtime = 0;

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
		strmove (begin, end);

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

				strmove (ptr + 1, ptr + 3);
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

void html_update_nodepage ()
{
	nodepage_mtime = time (NULL);
}

/*****************************************************************************/

static Connection *build_nodelist (Connection *c, Node *node, FILE *f)
{
	unsigned long shares = 0;
	double        size   = 0.0; /* MB */
	int           uploads = -1;
	int           max_uploads = -1;

	if (node->state != NODE_CONNECTED)
		return NULL;

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
	if (NODE (c)->class & NODE_USER /* NODE_CHILD */)
	{
		ft_share_stats_get_digest (node->ip, &shares, &size,
		                           &uploads, &max_uploads);
	}

	fprintf (f,
	         "</td><td>%s</td><td>%s%lu%s</td><td align='right'>"
	         "%s%.02fGB%s</td><td>%s%i / %i%s</td><td>%s</td></tr>\n",
	         node_class_str (node->class),
	         shares ? "" : "<!--", shares,               shares ? "" : "-->",
	         shares ? "" : "<!--", size / 1024.0,        shares ? "" : "-->",
			 shares ? " &nbsp; " : "<!--", uploads, max_uploads, shares ? " &nbsp; " : "-->",
	         (node->firewalled ? "[ firewalled ]" : ""));

	return NULL;
}

/* also, unoptimized crap. :) */
static int build_shares (FileShare *file, FILE *f)
{
	char *encoded;

	if (!file || !file->sdata)
		return TRUE;

	encoded = url_encode (file->sdata->hpath);

	fprintf (f, "<tr><td>%lu</td><td><a href='%s'>%s</a></td></tr>\n",
	         file->size, encoded, file->sdata->hpath);

	free (encoded);

	return TRUE;
}

char *html_page_index (char *file)
{
	FILE *f;
	struct stat st;
	int max_uploads;

	TRACE_FUNC ();

	if (stat (file, &st))
	{
		nodepage_mtime = time (NULL);
		st.st_mtime = 0;
	}

	/* nodepage_mtime is updated anytime the file should be rebuilt */
	if (nodepage_mtime == st.st_mtime)
		return file;

	/* can't update, so just deliver what we have */
	if (!(f = fopen (file, "w")))
		return file;

	/* write the header */
	fprintf (f,
			 "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n"
			 "<html>\n"
			 " <head>\n"
			 "  <title>giFT: OpenFT</title>\n"
			 "  <link rel='stylesheet' type='text/css' href='" IMAGE_HOST "/other.css'>\n"
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
	fprintf (f, "<h1><a href='http://www.giftproject.org/'>%s %s</a> "
				"(OpenFT revision: %hu.%hu.%hu)</h1>\n",
				PACKAGE, VERSION, OPENFT_MAJOR, OPENFT_MINOR, OPENFT_MICRO);
	fprintf (f, "<h1>Node Status: %s</h1>\n", node_class_str (-1));

	fprintf (f, "<p>Active Uploads: %i", upload_length (NULL));

	if ((max_uploads = share_status ()) != -1)
		fprintf (f, " / %i", max_uploads);

	fprintf (f, "</p>\n");

	fprintf (f,
	         "<h2>Node List</h2>\n"
	         "<table border='0' cellspacing='1' cellpadding='2'>\n"
	         "<tr>\n"
			 " <td><b> Node </b></td>\n"
			 " <td><b> Classification </b></td>\n"
			 " <td><b> Shares </b></td>\n"
			 " <td><b> Size </b></td>\n"
			 " <td><b> <!-- padding --> </b></td>\n"
			 " <td><b> <!-- firewalled --> </b></td>\n"
			 "</tr>\n");

	/* insert nodelist here */
	conn_foreach ((ConnForeachFunc) build_nodelist, f,
	              NODE_USER, NODE_CONNECTED, 0);

	fprintf (f,
	         "</table>\n"
	         "<h2>Shares</h2>\n"
	         "<table border='0' cellspacing='1' cellpadding='2'>\n"
	         "<tr><td><b>Filesize</b></td><td><b>Filename</b></td></tr>\n");

	/* insert shares list here */
	list_foreach_remove (share_index_sorted (),
	                     (ListForeachFunc) build_shares, f);

	fprintf (f,
	         "</table>\n"
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

	fclose (f);

	/* modify the global mtime */
	stat (file, &st);
	nodepage_mtime = st.st_mtime;

	return file;
}

/*****************************************************************************/

char *html_page_redirect (char *page)
{
	FILE *f;

	if (!(f = fopen (page, "w")))
		return NULL;

	fprintf (f, "<html><body>Redirection successful</body></html>\n");
	fclose (f);

	return page;
}
