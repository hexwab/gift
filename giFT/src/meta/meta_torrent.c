/*
 * $Id: meta_torrent.c,v 1.6 2004/04/30 13:36:50 mkern Exp $
 *
 * Copyright (C) 20003 giFT project (gift.sourceforge.net)
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

#include "giftd.h"

#include "plugin/share.h"

#include "lib/file.h"

#include <time.h>

/*****************************************************************************/

/* maximum size up to which we parse torrents */
#define MAX_TORRENT_SIZE (100 * 1024)

#if 0
# define PARSE_DEBUG(x) GIFT_DBG(x)
#else
# define PARSE_DEBUG(x)
#endif

/*****************************************************************************/

/**
 * We build a tree of BNodes from the torrent file.
 * See http://bitconjurer.org/BitTorrent/protocol.html for a description of
 * bencoding.
 */
typedef enum
{
	BSTR,                              /* string */
	BINT,                              /* integer */
	BLIST,                             /* list */
	BDICT,                             /* dictionary */
} BNodeType;

typedef struct
{
	/**
	 * Name of the node.  NULL for non-dictionary entries.
	 */
	char      *name;
	BNodeType  type;                   /* type of this node */

	union
	{
		/* integer, TODO: support for > 2^32 */
		long i;

		/* binary string */
		struct
		{
			int            len;
			unsigned char *data;
		} s;

		List *l;                       /* list (unnamed BNodes) */
		List *d;                       /* dictionary (named BNodes) */
	} data;
} BNode;

/*****************************************************************************/

typedef struct
{
	unsigned long length;              /* total size */
	unsigned long files;               /* number of files */
} MultiFile;

/*****************************************************************************/

static BNode *create_bnode (const char *name, BNodeType type)
{
	BNode *node;

	if (!(node = malloc (sizeof (BNode))))
		return NULL;

	node->name = gift_strdup (name);
	node->type = type;

	switch (node->type)
	{
	 case BSTR:   node->data.s.len = 0;
	              node->data.s.data = NULL;  break;
	 case BINT:   node->data.i = 0;          break;
	 case BLIST:  node->data.l = NULL;       break;
	 case BDICT:  node->data.d = NULL;       break;
	};

	return node;
}

static BOOL free_bnode (BNode *node, void *udata)
{
	if (node == NULL)
		return TRUE;

	switch (node->type)
	{
	 case BSTR:
		free (node->data.s.data);
		break;
	 case BINT:
		break;
	 case BLIST:
		/* recurse for each list entry */
		list_foreach_remove (node->data.l, LIST_FOREACH(free_bnode), NULL);
		break;
	 case BDICT:
		/* recurse for each list entry */
		list_foreach_remove (node->data.d, LIST_FOREACH(free_bnode), NULL);
		break;
	};

	free (node->name);
	free (node);

	/* remove link */
	return TRUE;
}

static void benc_free (BNode *node)
{
	if (node == NULL)
		return;

	free_bnode (node, NULL);
}

/*****************************************************************************/

static int dict_cmp_key (BNode *node, const char *key)
{
	return gift_strcmp (node->name, key);
}

static BNode *benc_find_dict_key (BNode *node, const char *key)
{
	List *item;

	if (node == NULL || node->type != BDICT)
		return NULL;

	item = list_find_custom (node->data.d, (void *)key,
	                         (CompareFunc)dict_cmp_key);

	if (item == NULL)
		return NULL;

	return item->data;
}

/*****************************************************************************/

static int bstrlen (const unsigned char *benc, long len, unsigned char delim)
{
	long i;

	for (i = 0; i < len; i++, benc++)
	{
		if (*benc == delim)
			return i;
	}

	return -1;
}

static BNode *bparse_recursive (unsigned char **benc, long *remaining)
{
	BNode *node = NULL;
	long len, pos;

	if (*remaining < 2)
		return NULL;

	switch (**benc)
	{
	 case 'i':
		/* integer */
		(*benc)++;
		(*remaining)--;

		PARSE_DEBUG(("benc: integer?"));

		if ((pos = bstrlen (*benc, *remaining, 'e')) < 1)
			return NULL;

		(*benc)[pos] = 0;

		if (!(node = create_bnode (NULL, BINT)))
			return NULL;

		node->data.i = gift_strtol (*benc);

		PARSE_DEBUG(("benc: integer! '%ld'", node->data.i));

		*benc += pos + 1;
		*remaining -= pos + 1;
		break;

	 case 'l':
		/* list */
		(*benc)++;
		(*remaining)--;

		PARSE_DEBUG(("benc: list?"));

		if (!(node = create_bnode (NULL, BLIST)))
			return NULL;

		while (*remaining > 0 && **benc != 'e')
		{
			/* parse list entry */
			BNode *child;

			if (!(child = bparse_recursive (benc, remaining)))
			{
				benc_free (node);
				return NULL;
			}

			/* insert into list */
			node->data.l = list_prepend (node->data.l, (void*) child);
		}

		if (*remaining < 1)
		{
			benc_free (node);
			GIFT_DBG (("meta_torrent: list remaining: %ld", *remaining));
			return NULL;
		}

		PARSE_DEBUG(("benc: list! %d entries", list_length (node->data.l)));

		/* skip trailing 'e' */
		(*benc)++;
		(*remaining)--;

		break;

	 case 'd':
		/* dictionary */
		(*benc)++;
		(*remaining)--;

		PARSE_DEBUG(("benc: dictionary?"));

		if (!(node = create_bnode (NULL, BDICT)))
			return NULL;

		while (*remaining > 0 && **benc != 'e')
		{
			BNode *key, *value;

			/* get key, must be a string */
			if (!(key = bparse_recursive (benc, remaining)) || key->type != BSTR)
			{
				benc_free (node);
				return NULL;
			}

			/* get data */
			if (!(value = bparse_recursive (benc, remaining)))
			{
				benc_free (key);
				benc_free (node);
				return NULL;
			}

			/* set name */
			value->name = strdup (key->data.s.data);
			benc_free (key);

			/* insert into list */
			node->data.d = list_prepend (node->data.d, (void*) value);
		}

		if (*remaining < 1)
		{
			benc_free (node);
			GIFT_DBG (("meta_torrent: dict remaining: %ld", *remaining));
			return NULL;
		}

		PARSE_DEBUG(("benc: dictionary! %d entries", list_length (node->data.d)));

		/* skip trailing 'e' */
		(*benc)++;
		(*remaining)--;

		break;

	 default:
		/* must be string */
		PARSE_DEBUG(("benc: string?"));

		if ((pos = bstrlen (*benc, *remaining, ':')) < 1)
			return NULL;

		(*benc)[pos] = 0;
		if ((len = gift_strtol (*benc)) < 0)
			return NULL;

		*benc += pos + 1;
		*remaining -= pos + 1;

		if (*remaining < len)
		{
			GIFT_DBG (("meta_torrent: string len: %ld, remaining: %ld",
			           len, *remaining));
			return NULL;
		}

		if (!(node = create_bnode (NULL, BSTR)))
			return NULL;

		if (!(node->data.s.data = malloc (sizeof (unsigned char) * (len + 1))))
		{
			benc_free (node);
			return NULL;
		}

		node->data.s.len = len;
		memcpy (node->data.s.data, *benc, len);
		node->data.s.data[len] = 0;

		PARSE_DEBUG(("benc: string! '%s'",
		            node->data.s.len < 100 ?
		            node->data.s.data :
		            stringf("[binary: len %d]", node->data.s.len)));

		*benc += len;
		*remaining -= len;

		break;
	}

	return node;
}

static BNode *benc_parse_file (const char *file)
{
	BNode         *node;
	unsigned char *data, *data0;
	unsigned long  len;
	long           slen;
	struct stat    st;

	if (file_stat (file, &st) == FALSE)
	{
		GIFT_WARN (("meta_torrent: can't stat %s: %s", file, GIFT_STRERROR()));
		return NULL;
	}

	if (st.st_size > MAX_TORRENT_SIZE)
	{
		GIFT_WARN (("meta_torrent: torrent too large: %s", file));
		return NULL;
	}

	if (file_slurp (file, (char **) &data, &len) == FALSE)
	{
		GIFT_WARN (("meta_torrent: can't open %s: %s", file, GIFT_STRERROR()));
		return NULL;
	}

	data0 = data;
	slen = (long)len;

	if (!(node = bparse_recursive (&data, &slen)))
	{
		GIFT_WARN (("meta_torrent: corrupt torrent: %s", file));
		free (data0);
		return NULL;
	}

	if (slen != 0)
	{
		GIFT_WARN (("meta_torrent: possibly corrupt torrent %s, %d bytes remain",
		            file, slen));
	}

	free (data0);

	return node;
}

/*****************************************************************************/

static BOOL handle_multi_file (BNode *file, MultiFile *multi)
{
	BNode *node;

	if (file == NULL || file->type != BDICT)
		return TRUE;

	multi->files++;

	if ((node = benc_find_dict_key (file, "length")) && node->type == BINT)
		multi->length += node->data.i;

#if 0
	if ((node = benc_find_dict_key (file, "path")) && node->type == BLIST)
		/* TODO: use use this somehow?  */
#endif

	return TRUE;
}

static BOOL handle_info_key (BNode *info, Share *share)
{
	BNode *node;

	/* decide between single and multi file case */
	if ((node = benc_find_dict_key (info, "files")) && node->type == BLIST)
	{
		/* multi file */
		MultiFile multi;

		multi.length = 0;
		multi.files = 0;

		list_foreach (node->data.l, LIST_FOREACH (handle_multi_file),
		              (void *)&multi);

		share_set_meta (share, "files",     gift_ultostr (multi.files));
		share_set_meta (share, "mediasize", gift_ultostr (multi.length));

		/* save directory name */
		if ((node = benc_find_dict_key (info, "name")) && node->type == BSTR)
			share_set_meta (share, "savename", node->data.s.data);
	}
	else if ((node = benc_find_dict_key (info, "length")) && node->type == BINT)
	{
		/* single file */
		share_set_meta (share, "files", "1");

		/* length */
		if (node->type == BINT)
			share_set_meta (share, "mediasize", gift_ltostr (node->data.i));

		/* save file name */
		if ((node = benc_find_dict_key (info, "name")) && node->type == BSTR)
			share_set_meta (share, "savename", node->data.s.data);
	}

	return TRUE;
}

static BOOL handle_toplevel_keys (BNode *node, Share *share)
{
	if (gift_strcmp (node->name, "announce") == 0 && node->type == BSTR)
		share_set_meta (share, "announce", node->data.s.data);
	else if (gift_strcmp (node->name, "comment") == 0 && node->type == BSTR)
		share_set_meta (share, "comment", node->data.s.data);
	else if (gift_strcmp (node->name, "info") == 0 && node->type == BDICT)
		handle_info_key (node, share);
	else if (gift_strcmp (node->name, "creation date") == 0 && node->type == BINT)
	{
		char buf[32];
		time_t seconds = (time_t)node->data.i;

		if (strftime (buf, 32, "%Y-%m-%d", gmtime (&seconds)) > 0)
			share_set_meta (share, "creationdate", buf);
	}

	return TRUE;
}


BOOL meta_torrent_run (Share *share, const char *path)
{
	BNode *node;

#if 0
	GIFT_DBG (("meta_torrent: %s", path));
#endif

	if (!(node = benc_parse_file (path)))
		return FALSE;

	if (node->type != BDICT)
	{
		GIFT_WARN (("meta_torrent: top level not dictionary: %s", path));
		benc_free (node);
		return FALSE;
	}

	/* extract interesting keys */
	list_foreach (node->data.d, LIST_FOREACH (handle_toplevel_keys),
	              (void *)share);

	benc_free (node);

	return TRUE;
}
