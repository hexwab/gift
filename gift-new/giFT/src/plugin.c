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

#include "event.h"    /* for input_remove_full */

#include "plugin.h"

/*****************************************************************************/

#ifndef USE_DLOPEN
int  OpenFT_init (Protocol *p);
/* # include "openft.h" <== this doesn't work when using --disble-libdl */
#endif

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

static Protocol *plugin_init (char *file, char *proto, char *i_func)
{
	Protocol     *p;
	void         *handle = NULL;
	ProtocolInit  init   = NULL;

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
	p = protocol_new ();
	p->name   = proto;
	p->handle = handle;

	/* initialize the plugin */
	if (!((*init) (p)))
	{
#ifdef USE_DLOPEN
		if (p->handle)
			platform_dlclose (p->handle);
#endif /* USE_DLOPEN */

		protocol_destroy (p);

		GIFT_FATAL (("%s (%s): init func failed\n",
		             proto, file));
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
	proto = parse_proto (file, &i_func);

	/* call the plugin's init function */
	p = plugin_init (file, proto, i_func);

	free (i_func);

	if (!p)
	{
		free (proto);
		return;
	}

	/* add this new protocol to the list */
	protocol_add (p);
}

/*****************************************************************************/

void plugin_unload (Protocol *p)
{
	if (!p)
		return;

	/* get the protocol to kill itself */
	if (p->destroy)
		p->destroy (p);

	/* remove all inputs from this protocol */
	input_remove_full (p, NULL, 0, NULL);

#ifdef USE_DLOPEN
	if (p->handle)
		platform_dlclose (p->handle);
#endif /* USE_DLOPEN */

	protocol_destroy (p);
}
