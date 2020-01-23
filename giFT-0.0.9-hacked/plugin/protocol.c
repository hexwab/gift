/*
 * $Id: protocol.c,v 1.10 2003/05/31 01:06:46 jasta Exp $
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

#include "lib/libgift.h"

#include "protocol.h"

/*****************************************************************************/

static Dataset *protocols = NULL;

/*****************************************************************************/

static BOOL  dummy_bool ()      { return FALSE; }
static BOOL  dummy_bool_true () { return TRUE; }
static int   dummy_int ()       { return ((int)dummy_bool()); }
static void  dummy_void ()      { }
static void *dummy_voidptr ()   { return NULL; }

/*****************************************************************************/

int protocol_compat (uint32_t version)
{
	int   ret;
	char *quality;

	ret = INTCMP (version, LIBGIFTPROTO_VERSION);

	if (ret > 0)
		quality = "older";
	else if (ret < 0)
		quality = "newer";
	else
		quality = NULL;

	if (quality)
	{
		GIFT_ERROR (("libgiftproto is %s than linked daemon or plugin, see "
		             "following messages for more information", quality));
	}

	return ret;
}

/*****************************************************************************/

static void setup_functbl (Protocol *p)
{
#ifdef _MSC_VER
/*
 * Shuts up:
 * warning C4113: 'int (__cdecl *)()' differs in parameter lists from
 * 'int (__cdecl *)(struct _protocol *,char *,unsigned char *(__cdecl *)(char *,
 * char *,int *),char *(__cdecl *)(unsigned char *,int *))'
 */
#pragma warning(disable: 4113)
#endif /* _MSC_VER */

	/* initialize all the giFT space callbacks for OpenFT -> giFT
	 * communication */
	p->init            = dummy_bool;
	p->hash_handler    = dummy_bool;
	p->support         = dummy_void;
	p->chunk_write     = dummy_void;
	p->search_result   = dummy_void;
	p->search_complete = dummy_void;
	p->message         = dummy_void;

	/* initialize the functions which the plugin is expected to assign to more
	 * useful values, this is done so that we can be assured we wont have to
	 * check for NULL when calling these */
	p->start           = dummy_bool;
	p->destroy         = dummy_void;
	p->download_start  = dummy_bool;
	p->download_stop   = dummy_void;
	p->upload_stop     = dummy_void;
	p->upload_avail    = dummy_void;
	p->chunk_suspend   = dummy_bool;
	p->chunk_resume    = dummy_bool;
	p->source_remove   = dummy_bool;
	p->source_status   = dummy_void;
	p->source_cmp      = dummy_int;
	p->user_cmp        = dummy_int;
	p->search          = dummy_bool;
	p->browse          = dummy_bool;
	p->locate          = dummy_bool;
	p->search_cancel   = dummy_void;
	p->share_new       = dummy_voidptr;
	p->share_free      = dummy_void;
	p->share_add       = dummy_bool_true;
	p->share_remove    = dummy_bool_true;
	p->share_sync      = dummy_void;
	p->share_hide      = dummy_void;
	p->share_show      = dummy_void;
	p->stats           = dummy_int;
}

Protocol *protocol_new (char *name, uint32_t version)
{
	Protocol *p;

	if (protocol_compat (version) != 0)
	{
		GIFT_FATAL (("runtime incompatibility detected!"));
		return NULL;
	}

	if (!(p = MALLOC (sizeof (Protocol))))
		return NULL;

	if (!(p->name = STRDUP (name)))
	{
		free (p);
		return NULL;
	}

	/* setup the initial function table */
	setup_functbl (p);

	/* add to the list of protocols */
	assert (dataset_lookupstr (protocols, name) == NULL);
	dataset_insert (&protocols, name, STRLEN_0 (name), p, 0);

	return p;
}

static void free_proto (Protocol *p)
{
	free (p->name);
	free (p);
}

void protocol_free (Protocol *p)
{
	if (!p)
		return;

	dataset_removestr (protocols, p->name);
	free_proto (p);
}

/*****************************************************************************/

Protocol *protocol_lookup (char *name)
{
	return dataset_lookupstr (protocols, name);
}

void protocol_foreach (DatasetForeachFn func, void *udata)
{
	dataset_foreach (protocols, func, udata);
}

void protocol_foreachclear (DatasetForeachExFn func, void *udata)
{
	dataset_foreach_ex (protocols, func, udata);
}
