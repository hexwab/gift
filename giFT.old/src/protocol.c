/*
 * protocol.c
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

#ifdef USE_DLOPEN
#include <dlfcn.h>
#endif

#ifndef USE_DLOPEN
#include "openft.h"
#endif

/*****************************************************************************/

/**/extern List *protocols;

/*****************************************************************************/

Protocol *protocol_new ()
{
	Protocol *p;

	if (!(p = malloc (sizeof (Protocol))))
	{
		GIFT_ERROR (("out of memory"));
		return NULL;
	}

	memset (p, 0, sizeof (Protocol));

	return p;
}

void protocol_destroy (Protocol *p)
{
	free (p->name);

#if USE_DLOPEN
	if (p->handle)
		dlclose (p->handle);
#endif /* USE_DLOPEN */

	dataset_clear (p->support);

	free (p);
}

Protocol *protocol_find (char *name)
{
	List *temp;

	if (!name)
		return NULL;

	for (temp = protocols; temp; temp = temp->next)
	{
		Protocol *p = temp->data;

		if (!strcmp (p->name, name))
			return p;
	}

	return NULL;
}

/* load the protocol from file */
Protocol *protocol_load (char *file_name)
{
	Protocol    *p;
	void        *handle = NULL;
	char        *proto;
	char        *init_func;
	ProtocolInit init;

#if USE_DLOPEN
	if (!(handle = dlopen (file_name, RTLD_LAZY)))
	{
		GIFT_FATAL (("Couldn't load protocol in file %s: %s\n",
					 file_name, dlerror ()));
		return NULL;
	}
#endif /* USE_DLOPEN */

	/* construct the init function name */
	if ((proto = strrchr (file_name, '/')))
		proto++;
	else
		proto = file_name;

	if (!strncmp (proto, "lib", 3))
		proto += 3;

	proto = strdup (proto);

	if ((init_func = strchr (proto, '.')))
		*init_func = 0;

	init_func = malloc (strlen (proto) + 6); /* _init\0 */
	sprintf (init_func, "%s_init", proto);

#if USE_DLOPEN
	/* retrieve the symbol */
	init = dlsym (handle, init_func);
#else
	if (!strcmp (proto, "OpenFT"))
		init = OpenFT_init;
#endif /* USE_DLOPEN */

	free (init_func);

#if USE_DLOPEN
	if (!init)
	{
		free (proto);
		GIFT_FATAL (("Couldn't load protocol in file %s\n", dlerror ()));
		return NULL;
	}
#endif /* USE_DLOPEN */

	p = protocol_new ();
	p->name   = proto;
	p->handle = handle;

	if (!((*init) (p)))
	{
		protocol_destroy (p);
		GIFT_FATAL (("Couldn't load protocol in file %s\n", file_name));
		return NULL;
	}

	return p;
}

void protocol_unload (Protocol *p)
{
	if (p->destroy)
		p->destroy (p);

	protocol_destroy (p);
}

/*****************************************************************************/

#if 0
Protocol *protocol_add_event (Protocol *p, char *name,int numargs, ...)
{
	va_list argptr;
	Event  *e;
	char   *arg;
	char   *tmp;
	int     i = 0;

	if (name == NULL)
		return p;

	va_start (argptr, numargs);
	e = event_new (name); /* Create a new event */

	while (i < numargs)
	{
		EventArg* ea = NULL;
		i += 2; /* Move by 2, arg type pair */

		tmp = va_arg (argptr, char *);

		if (tmp == NULL)
			return p;

		arg = strdup (tmp);
		if (!arg)
			return p; /* Out of memory? */

		switch (va_arg (argptr, int))
		{
		case OPT_TYPE_INT:
			ea = event_arg_new (arg,"int");
			break;
		case OPT_TYPE_FLAG:
			ea = event_arg_new (arg,"flag");
			break;
		case OPT_TYPE_STRING:
			ea = event_arg_new (arg,"string");
			break;
		default:
	    /* Someone borked */
			return p;
			break;
		}

		free (arg);

		if (ea)
			e->args = list_append (e->args, ea);
	}

	p->events = list_append (p->events,e);

	return p;
}
#endif
