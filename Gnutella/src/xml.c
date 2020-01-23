/*
 * $Id: xml.c,v 1.10 2004/04/13 07:25:18 hipnod Exp $
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

#include <zlib.h>

#ifdef USE_LIBXML2
#include <libxml/parser.h>         /* xmlParseMemory() */
#include <libxml/xmlerror.h>       /* xmlSetGenericErrorFunc() */
#endif /* USE_LIBXML2 */

#include "xml.h"

/*****************************************************************************/

#ifndef USE_LIBXML2
BOOL gt_xml_parse (const char *xml, Dataset **ret)
{
	return FALSE;
}

BOOL gt_xml_parse_indexed (const char *xml, size_t bin_len, Share **shares,
                           size_t shares_len)
{
	return FALSE;
}
#endif /* !USE_LIBXML2 */

/*****************************************************************************/

/* the rest of this file is conditional on using libxml */
#ifdef USE_LIBXML2

/*****************************************************************************/

#define MAX_XML_BUFSIZE  65536

static char      *xml_buf;     /* for decompressing xml */
static size_t     xml_buf_size;
static z_stream   zxml;

/*****************************************************************************/

static void print_nodes (xmlNodePtr node, Dataset **ret)
{
	while (node != NULL)
	{
		/*
		 * If this node has no children, it is a leaf node,
		 * so set the metadata from it.
		 */
		if (node->xmlChildrenNode)
			print_nodes (node->xmlChildrenNode, ret);
		else
			GT->DBGFN (GT, "name=%s", node->name);

		node = node->next;
	}
}

BOOL gt_xml_parse (const char *xml, Dataset **ret)
{
	xmlDocPtr doc;

	/* disable for now because it doesn't work anyway: need to share
	 * code with parse_indexed */
	if (!XML_DEBUG)
		return FALSE;

	/* only parse documents starting with '<' */
	if (!xml || xml[0] != '<')
		return FALSE;

	if (!(doc = xmlParseMemory (xml, strlen (xml))))
		return FALSE;

	print_nodes (doc->xmlChildrenNode, ret);

	xmlFreeDoc (doc);

	return TRUE;
}

static void add_child (Dataset **children, const char *key, const char *value)
{
	char *dup = NULL;

	if (!key || !value)
		return;

	/*
	 * Hack to map some of the attributes from XML documents found
	 * on Gnutella to ones peddled by giFT.
	 */
	if (!strcasecmp (key, "bitrate"))
	{
		dup = stringf_dup ("%s000", value);
		value = dup;
	}
	else if (!strcasecmp (key, "seconds"))
	{
		key = "duration";
	}

	dataset_insertstr (children, key, value);
	free (dup);
}

static Dataset *collect_attributes (xmlNode *node)
{
	const xmlAttr *attr;
	Dataset       *children = NULL;
	BOOL           do_log   = XML_DEBUG;

	for (attr = node->properties; attr != NULL; attr = attr->next)
	{
		xmlChar *str;

		/* is there an easier way to get attribute content? */
		str = xmlGetProp (node, attr->name);

		if (do_log)
		{
			GT->dbg (GT, "name=%s content=%s",
			         (const char *)attr->name, (const char *)str);
		}

		/* add the key->value pair to the dataset */
		add_child (&children, (const char *)attr->name,
		           (const char *)str);

		/* xmlGetProp() allocates memory */
		free (str);
	}

	return children;
}

static void set_meta_foreach (ds_data_t *key, ds_data_t *value, Share *share)
{
	char *meta_key = key->data;
	char *meta_val = value->data;

	share_set_meta (share, meta_key, meta_val);
}

static void set_share_meta (Share **shares, size_t shares_len,
                            Dataset *children)
{
	char      *index_str;
	size_t     index;

	/*
	 * Lookup the "index" attribute, and use that to determine
	 * which Share the XML applies to.
	 */
	if (!(index_str = dataset_lookupstr (children, "index")))
		return;

	index = gift_strtoul (index_str);

	if (index >= shares_len)
		return;

	if (!shares[index])
		return;

	/* skip the index attribute */
	dataset_removestr (children, "index");

	dataset_foreach (children, DS_FOREACH(set_meta_foreach), shares[index]);
}

static void set_metadata_from_indexed_xml (Share **shares, size_t shares_len,
                                           xmlDoc *doc)
{
	xmlNode *node;

	if (!(node = xmlDocGetRootElement (doc)))
		return;

	for (node = node->xmlChildrenNode; node != NULL; node = node->next)
	{
		Dataset *children;

		children = collect_attributes (node);

		set_share_meta (shares, shares_len, children);
		dataset_clear (children);
	}
}

static int try_inflate_xml (const char *xml, size_t bin_len)
{
	int ret;

	/* set zlib allocation data */
	zxml.zalloc    = Z_NULL;
	zxml.zfree     = Z_NULL;
	zxml.opaque    = Z_NULL;

	/* set the input parameters */
	zxml.next_in   = (char *)xml;
	zxml.avail_in  = bin_len;

	/* set the output parameters */
	zxml.next_out  = xml_buf;
	zxml.avail_out = xml_buf_size - 1;

	if ((ret = inflateInit (&zxml)) != Z_OK)
		return ret;

	ret = inflate (&zxml, Z_FINISH);
	inflateEnd (&zxml);

	return ret;
}

static const char *inflate_xml (const char *xml, size_t bin_len)
{
	size_t xml_len;
	int    ret;

	ret = try_inflate_xml (xml, bin_len);

	if (ret == Z_BUF_ERROR && xml_buf_size < MAX_XML_BUFSIZE)
	{
		size_t newsize = xml_buf_size * 2;
		char  *newbuf;

		if (!(newbuf = realloc (xml_buf, newsize)))
			return NULL;

		xml_buf      = newbuf;
		xml_buf_size = newsize;

		/* retry with bigger buffer */
		return inflate_xml (xml, bin_len);
	}

	if (ret != Z_STREAM_END)
		return NULL;

	/* null terminate (the now hopefully plaintext) XML */
	xml_len = (xml_buf_size - 1) - zxml.avail_out;
	xml_buf[xml_len] = 0;

	if (XML_DEBUG)
		GT->dbg (GT, "inflated xml: %s", xml_buf);

	return xml_buf;
}

BOOL gt_xml_parse_indexed (const char *xml, size_t bin_len, Share **shares,
                           size_t shares_len)
{
	xmlDoc     *doc;
	size_t      xml_len;
	const char *next;
	const char *ptr;

	if (!xml || bin_len <= 4)
		return FALSE;

	/*
	 * Look for the encoding type, currently possible
	 * encoding values are: "{}" meaning plain text, "{plaintext}",
	 * and "{deflate}".
	 */

	if (!strncmp (xml, "{}", 2))
	{
		xml += 2;
	}
	else if (bin_len >= sizeof("{plaintext}") - 1 &&
	         !strncasecmp (xml, "{plaintext}", sizeof("{plaintext}") - 1))
	{
		xml += sizeof("{plaintext}") - 1;
	}
	else if (bin_len >= sizeof("{deflate}") - 1 &&
	         !strncasecmp (xml, "{deflate}", sizeof("{deflate}") - 1))
	{
		/* the len passed here should be bin_len - 1, but some servents (MRPH)
		 * don't terminate the XML */
		xml = inflate_xml (xml + sizeof("{deflate}") - 1, bin_len);

		if (XML_DEBUG)
			assert (xml != NULL);    /* assume valid input */

		if (!xml)
			return FALSE;
	}

	xml_len = strlen (xml);

	/*
	 * The XML block is a sequence of XML documents, separated by the <?xml
	 * version="1.0"> document prefix.  Parse each one separately.
	 */
	for (ptr = xml; ptr != NULL; ptr = next)
	{
		size_t chunk_len;

		if (ptr[0] != '<')
			return FALSE;

		next = strstr (ptr + 1, "<?xml");

		chunk_len = xml_len;
		if (next)
			chunk_len = next - ptr;

		if (!(doc = xmlParseMemory (ptr, chunk_len)))
			return FALSE;

		xml_len -= chunk_len;

		set_metadata_from_indexed_xml (shares, shares_len, doc);
		xmlFreeDoc (doc);
	}

	return TRUE;
}

/* gets called when there are parsing errors */
static void error_handler_func (void *udata, const char *msg, ...)
{
	char     buf[1024];
	va_list  args;

	/* this is here until i figure out why i get a message about
	 * namespace errors (but it still seems to work...) */
	if (!XML_DEBUG)
		return;

	va_start (args, msg);
	vsnprintf (buf, sizeof (buf) - 1, msg, args);
	va_end (args);

	GT->DBGFN (GT, "xml parse error: %s", buf);
}

/*****************************************************************************/

#endif /* USE_LIBXML2 */

/*****************************************************************************/

void gt_xml_init (void)
{
#ifdef USE_LIBXML2
	/* so libxml doesn't print messages on stderr */
	xmlSetGenericErrorFunc (NULL, error_handler_func);

	xml_buf = malloc (32);
	assert (xml_buf != NULL);
	xml_buf_size = 32;

	memset (&zxml, 0, sizeof (zxml));
#endif /* USE_LIBXML2 */
}

void gt_xml_cleanup (void)
{
#ifdef USE_LIBXML2
	free (xml_buf);
	xml_buf      = NULL;
	xml_buf_size = 0;
#endif /* USE_LIBXML2 */
}
