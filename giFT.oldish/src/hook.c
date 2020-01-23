/*
 * hook.c
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
#include "hook.h"

#ifdef USE_PERL
#include "perlc.h"
#endif

/*****************************************************************************/

#define APPEND args_str + args_len, sizeof (args_str) - args_len - 1

HookVar *hook_event (char *name, int ret, ...)
{
#if defined (USE_PERL) /* || defined (PADDING) */
	va_list     args;
	HookVarType arg_type;
	char        args_str[2048]; /* TODO */
	int         args_len = 0;
	HookVar    *hv = NULL;

	va_start (args, ret);

	/* construct the argument list for the scripting language */
	for (;;)
	{
		if ((arg_type = va_arg (args, HookVarType)) == HOOK_VAR_NUL)
			break;

		if (args_len)
			args_len += snprintf (APPEND, ", ");

		switch (arg_type)
		{
		 case HOOK_VAR_INT:
			{
				long arg = va_arg (args, long); /* promote */
				args_len += snprintf (APPEND, "\"%li\"", arg);
			}
			break;
		 case HOOK_VAR_STR:
			{
				char *arg = va_arg (args, char *);
				args_len += snprintf (APPEND, "\"%s\"", arg);
			}
			break;
		 default:
			break;
		}
	}

	va_end (args);

#if 0
	TRACE (("%s: %s", name, args_str));
#endif

# ifdef USE_PERL
	hv = perl_hook_event (name, args_str);
# endif /* USE_PERL */

	/* if they dont really want the return value conveniently free it here
	 * and return NULL */
	if (!ret)
	{
		hook_var_free (hv);
		hv = NULL;
	}

	return hv;
#else
	return NULL;
#endif /* USE_PERL */
}

/*****************************************************************************/

HookVar *hook_var_new (HookVarType type, void *value)
{
	HookVar *hv;

	if (type == HOOK_VAR_NUL || !value)
		return NULL;

	if (!(hv = malloc (sizeof (HookVar))))
		return NULL;

	memset (hv, 0, sizeof (HookVar));

	hv->type = type;

	switch (hv->type)
	{
	 case HOOK_VAR_INT:
		hv->value.num = (int) value;
		break;
	 case HOOK_VAR_STR:
		hv->value.str = STRDUP (value);
		break;
	 default:
		TRACE (("unknown type %i", hv->type));
		break;
	}

	return hv;
}

void hook_var_free (HookVar *hv)
{
	if (!hv)
		return;

	if (hv->type == HOOK_VAR_STR)
		free (hv->value.str);

	free (hv);
}

/*****************************************************************************/

void *hook_var_value (HookVar *hv)
{
	void *ret = NULL;

	if (!hv)
		return NULL;

	switch (hv->type)
	{
	 case HOOK_VAR_INT:
		ret = (void *) hv->value.num;
		break;
	 case HOOK_VAR_STR:
		ret = (void *) hv->value.str;
		break;
	 default:
		break;
	}

	return ret;
}
