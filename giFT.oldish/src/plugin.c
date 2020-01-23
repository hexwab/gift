/*
 * plugin.c
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

#include "gift.h"

#include "event.h"
#include "plugin.h"

/* protocol functbl */
#include "share_hash.h"
#include "share_file.h"
#include "if_search.h"
#include "if_message.h"
#include "transfer.h"

/*****************************************************************************/

#ifndef USE_DLOPEN
int OpenFT_init (Protocol *p);
#endif /* !USE_DLOPEN */

/*****************************************************************************/

#if defined (WIN32) || defined (USE_DLOPEN)
# ifndef WIN32
#  include <dlfcn.h>
# else
#  define RTLD_LAZY 0
# endif
#endif /* WIN32 || USE_DLOPEN */

#if 0
void       *platform_dlopen   (const char *filename, int flags);
const char *platform_dlerror  (void);
void       *platform_dlsym    (void *handle, char *symbol);
int         platform_dlclose  (void *handle);
#endif

/*****************************************************************************/
/* dynaloading (previously found in platform.c, but moved so that user
 * interfaces do not need to link libdl) */

#if defined (WIN32) || defined (USE_DLOPEN)

void *platform_dlopen (const char *filename, int flags)
{
#ifndef WIN32
	return dlopen (filename, flags);
#else /* WIN32 */
	return LoadLibrary (filename);
#endif /* !WIN32 */
}

const char *platform_dlerror (void)
{
#ifndef WIN32
	return dlerror ();
#else /* WIN32 */
	return platform_error ();
#endif /* !WIN32 */
}

void *platform_dlsym (void *handle, char *symbol)
{
#ifndef WIN32
	return dlsym (handle, symbol);
#else /* WIN32 */
	return (void *) GetProcAddress (handle, symbol);
#endif /* !WIN32 */
}

/* NOTE: this function returns TRUE or FALSE to indicate error status.  Not
 * traditional UNIX return. */
int platform_dlclose (void *handle)
{
#ifndef WIN32
	return (dlclose (handle) == 0) ? TRUE : FALSE;
#else /* WIN32 */
	return FreeLibrary (handle);
#endif /* !WIN32 */
}

#endif /* WIN32 || USE_DLOPEN */

/*****************************************************************************/

static int hash_set_wrapper (Protocol *p, char *type,
                             unsigned char* (*algo) (char *path, char *type,
                                                     int *len),
                             char* (*human) (unsigned char *hash, int *len))
{
	return hash_algo_register (p, type, (HashAlgorithm)algo, (HashHuman)human);
}

static void support_wrapper (Protocol *p, char *feature, int spt)
{
	dataset_insert (&p->features, feature, STRLEN_0 (feature),
	                &spt, sizeof (spt));
}

static void search_result_wrapper (Protocol *p, IFEvent *event, char *user,
                                   char *node, char *href, unsigned int avail,
                                   FileShare *file)
{
	if_search_result (event, user, node, href, avail, file);
}

static void search_complete_wrapper (Protocol *p, IFEvent *event)
{
	if_search_remove (event, p);
}

static void chunk_write_wrapper (Protocol *p, Transfer *transfer,
                                 Chunk *chunk, Source *source,
                                 unsigned char *data, size_t len)
{
	chunk_write (chunk, data, len);
}

static void message_wrapper (Protocol *p, char *message)
{
	if_message_send (NULL, p, message);
}

/*****************************************************************************/

static void source_status_default (Protocol *p, Source *source,
                                   unsigned short klass, char *disptxt)
{
	source_status_set (source, (SourceStatus)klass, disptxt);
}

static int user_cmp_default (Protocol *p, char *a, char *b)
{
	return STRCMP (a, b);
}

static int source_cmp_default (Protocol *p, Source *a, Source *b)
{
	int ret;

	assert (a != NULL);
	assert (b != NULL);

	if ((ret = STRCMP (a->url, b->url)))
		return ret;

	if ((ret = STRCMP (a->hash, b->hash)))
		return ret;

	return user_cmp_default (p, a->user, b->user);
}

/*****************************************************************************/

static char *parse_proto (char *file, char **i_func)
{
	char *proto;
	char *init_func;

	/* construct the init function name */
	if ((proto = strrchr (file, '/')))
		proto++;
	else
		proto = file;

	if (!strncmp (proto, "lib", 3))
		proto += 3;

	proto = strdup (proto);

	if ((init_func = strchr (proto, '.')))
		*init_func = 0;

	init_func = malloc (strlen (proto) + 6); /* _init\0 */
	sprintf (init_func, "%s_init", proto);

	if (i_func)
		*i_func = init_func;

	return proto;
}

static void setup_functbl (Protocol *p, int (*init) (Protocol *p),
                           void *handle)
{
	p->init            = init;
	p->handle          = handle;
	p->hash_set        = hash_set_wrapper;
	p->support         = support_wrapper;
	p->search_result   = search_result_wrapper;
	p->search_complete = search_complete_wrapper;
	p->chunk_write     = chunk_write_wrapper;
	p->source_status   = source_status_default;
	p->source_cmp      = source_cmp_default;
	p->user_cmp        = user_cmp_default;
	p->message         = message_wrapper;
}

static Protocol *plugin_init (char *file, char *proto, char *i_func)
{
	Protocol    *p;
	void        *handle = NULL;
	int        (*init) (Protocol *p);

#ifdef USE_DLOPEN
	if (!(handle = platform_dlopen (file, RTLD_LAZY)))
	{
		GIFT_FATAL (("couldn't load protocol in file %s: %s\n",
		             file, platform_dlerror ()));
		return NULL;
	}

	init = platform_dlsym (handle, i_func);
#else /* !USE_DLOPEN */
	if (!strcmp (proto, "OpenFT"))
		init = OpenFT_init;
#endif /* USE_DLOPEN */

	if (!init)
	{
#ifdef USE_DLOPEN
		GIFT_FATAL (("couldn't load protocol %s in file %s: %s",
		             proto, file, platform_dlerror ()));
#else
		GIFT_FATAL (("couldn't load protocol %s in file %s: %s",
		             proto, file, "protocol not supported by this binary"));
#endif

		return NULL;
	}

	/* create the protocol structure and store this data */
	if (!(p = protocol_new (proto)))
		return NULL;

	setup_functbl (p, init, handle);

	/* initialize the plugin */
	if (!(p->init (p)))
	{
		GIFT_FATAL (("%s (%s): init func failed\n",
		             proto, file));

#ifdef USE_DLOPEN
		if (handle)
			platform_dlclose (handle);
#endif /* USE_DLOPEN */

		protocol_free (p);
		return NULL;
	}

	return p;
}

void plugin_load (char *file)
{
	Protocol *p;
	char     *proto;
	char     *i_func;

	/* retrieve the protocol name and initialization function */
	if (!(proto = parse_proto (file, &i_func)))
		return;

	/* call the plugin's init function */
	if ((p = plugin_init (file, proto, i_func)))
	{
		/* TODO: we can do so much more useful things with the separation
		 * of init and start...dont be so lame :) */
		assert (p->start (p) == TRUE);
	}

	free (i_func);
	free (proto);
}

/*****************************************************************************/

void plugin_unload (Protocol *p)
{
	if (!p)
		return;

	/* get the protocol to kill itself */
	p->destroy (p);

	/* remove all inputs from this protocol */
	input_remove_full (p, NULL, 0, NULL);

#ifdef USE_DLOPEN
	if (p->handle)
		platform_dlclose (p->handle);
#endif /* USE_DLOPEN */

	protocol_free (p);
}
