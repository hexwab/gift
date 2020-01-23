/*
 * perl.c -- Thank you GAIM/X-Chat
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
#undef PACKAGE

#ifdef USE_PERL

# include <EXTERN.h>
# ifndef _SEM_SEMUN_UNDEFINED
#  define HAS_UNION_SEMUN
# endif
# undef MIN
# undef MAX
# undef DEBUG
# include <perl.h>
# include <XSUB.h>
# include <sys/mman.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
# undef PACKAGE
# include <stdio.h>
# include <dirent.h>
# include <string.h>

/*****************************************************************************/

struct perlscript
{
	char *name;
	char *version;
	char *shutdowncallback; /* bleh */
};

struct _perl_hook
{
	char *name;
	char *handler;
};

#if 0
struct _perl_timeout_handlers
{
	char *handler_name;
	int iotag;
};
#endif

/*****************************************************************************/

/* perl module support */
extern void xs_init _((void));
extern void boot_DynaLoader _((CV * cv)); /* perl is so wacky */

# undef _

/*****************************************************************************/

static List            *perl_list = NULL;
static List            *perl_hooks = NULL;
#if 0
static List *perl_timeout_handlers = NULL;
#endif
static PerlInterpreter *my_perl = NULL;

/*****************************************************************************/

/* dealing with giFT */
XS (XS_giFT_register);
XS (XS_giFT_log);
XS (XS_giFT_add_hook);

/*****************************************************************************/

void xs_init ()
{
	char *file = __FILE__;
	newXS ("DynaLoader::boot_DynaLoader", boot_DynaLoader, file);
}

static char *escape_quotes (char *buf)
{
	static char *tmp_buf = NULL;
	char *i, *j;

	if (tmp_buf)
		free(tmp_buf);

	assert (buf);

	tmp_buf = malloc (strlen (buf) * 2 + 1);

	for (i = buf, j = tmp_buf; *i; i++, j++)
	{
		if (*i == '\'' || *i == '\\')
			*j++ = '\\';

		*j = *i;
	}

	*j = '\0';

	return tmp_buf;
}

static SV *execute_perl_escaped (char *function, int args_quoted, char *args)
{
	static char *perl_cmd = NULL;

	if (perl_cmd)
		free (perl_cmd);

	assert (function);
	assert (args);

	perl_cmd = malloc (strlen (function) + strlen (args) + 10);
	sprintf (perl_cmd, "&%s(%s%s%s)", function,
			 (args_quoted ? "" : "'"), args, (args_quoted ? "" : "'"));

#ifndef HAVE_PERL_EVAL_PV
	return perl_eval_pv (perl_cmd, TRUE);
#else
	return Perl_eval_pv (perl_cmd, TRUE);
#endif
}

static SV *execute_perl (char *function, char *args)
{
	return execute_perl_escaped (function, FALSE, escape_quotes (args));
}

/*****************************************************************************/

int perl_load_file (char *script_name)
{
	SV *return_val;
	TRACE (("loading perl script %s...", script_name));
	return_val = execute_perl ("load_file", script_name);
	return SvNV (return_val);
}

#if 0
static int is_pl_file (char *filename)
{
	char *ext;

	if (!filename || !filename[0])
		return FALSE;

	if (!(ext = strrchr (filename, '.')))
		return FALSE;

	ext++;

	return (strcmp (ext, "pl") == 0);
}
#endif

void perl_autoload ()
{
	perl_load_file (gift_conf_path ("gift.pl"));
}

void perl_init()
{
	char *perl_args[] = { "", "-e", "0", "-w" };
	char load_file[] =
"sub load_file()\n"
"{\n"
"	(my $file_name) = @_;\n"
"	open FH, $file_name or return 2;\n"
"	my $is = $/;\n"
"	local($/) = undef;\n"
"	$file = <FH>;\n"
"	close FH;\n"
"	$/ = $is;\n"
"	$file = \"\\@ISA = qw(Exporter DynaLoader);\\n\" . $file;\n"
"	eval $file;\n"
"	eval $file if $@;\n"
"	return 1 if $@;\n"
"	return 0;\n"
"}";

	my_perl = perl_alloc ();
	perl_construct (my_perl);
	perl_parse (my_perl, xs_init, 4, perl_args, NULL);

#ifndef HAVE_PERL_EVAL_PV
	perl_eval_pv(load_file, TRUE);
#else
	Perl_eval_pv(load_file, TRUE);
#endif

	/* register all giFT-supplied functions */
	newXS ("giFT::register", XS_giFT_register, "giFT");
	newXS ("giFT::log",      XS_giFT_log,      "giFT");
	newXS ("giFT::add_hook", XS_giFT_add_hook, "giFT");
}

/*****************************************************************************/

void perl_cleanup ()
{
	List *ptr;
	struct perlscript *scp;
	struct _perl_hook *ehn;

	for (ptr = perl_list; ptr; ptr = list_next (ptr))
	{
		scp = ptr->data;

		if (scp->shutdowncallback && *scp->shutdowncallback)
			execute_perl (scp->shutdowncallback, "");

		free (scp->name);
		free (scp->version);
		free (scp->shutdowncallback);
		free (scp);
	}

	perl_list = list_free (perl_list);

	for (ptr = perl_hooks; ptr; ptr = list_next (ptr))
	{
		ehn = ptr->data;

		free (ehn->name);
		free (ehn->handler);
		free (ehn);
	}

	perl_hooks = list_free (perl_hooks);

#if 0
	while (perl_timeout_handlers) {
		thn = perl_timeout_handlers->data;
		perl_timeout_handlers = g_list_remove(perl_timeout_handlers, thn);
		gtk_timeout_remove(thn->iotag);
		g_free(thn->handler_name);
		g_free(thn);
	}
#endif

	if (my_perl)
	{
		perl_destruct (my_perl);
		perl_free (my_perl);
		my_perl = NULL;
	}
}

/*****************************************************************************/

HookVar *perl_hook_event (char *name, char *args)
{
	List              *ptr;
	struct _perl_hook *hook;
	SV                *ret    = NULL;
	SV                *ret_sv = NULL;
	HookVar           *ret_hv = NULL;

	for (ptr = perl_hooks; ptr; ptr = list_next (ptr))
	{
		hook = ptr->data;

		if (strcmp (hook->name, name))
			continue;

		if ((ret = execute_perl_escaped (hook->handler, TRUE, args)))
			ret_sv = ret;
	}

	/* fooey */
	if (!ret_sv)
		return NULL;

	switch (SvTYPE (ret_sv))
	{
	 case SVt_IV:
	 case SVt_NV:
		ret_hv = hook_var_new (HOOK_VAR_INT, I_PTR (SvIV (ret_sv)));
		break;
	 case SVt_PV:
		ret_hv = hook_var_new (HOOK_VAR_STR, (void *) SvPV_nolen (ret_sv));
		break;
	 default:
		TRACE (("invalid type %i", SvTYPE (ret_sv)));
		break;
	}

	return ret_hv;
}

/*****************************************************************************/

XS (XS_giFT_register)
{
	char *name, *ver, *callback, *unused; /* exactly like X-Chat, eh? :) */
	int junk;
	struct perlscript *scp;
	dXSARGS;
	items = 0;

	name = SvPV (ST (0), junk);
	ver = SvPV (ST (1), junk);
	callback = SvPV (ST (2), junk);
	unused = SvPV (ST (3), junk);

	scp = malloc (sizeof (struct perlscript));
	scp->name = strdup(name);
	scp->version = strdup(ver);
	scp->shutdowncallback = strdup(callback);
	perl_list = list_append(perl_list, scp);

	XST_mPV (0, VERSION);
	XSRETURN (1);
}

XS (XS_giFT_log)
{
	char *msg;
	int junk;
	dXSARGS;
	items = 0;

	msg = SvPV (ST (1), junk);

	TRACE (("%s", msg));

	XSRETURN (0);
}

XS (XS_giFT_add_hook)
{
	int junk;
	struct _perl_hook *hook;
	dXSARGS;
	items = 0;

	hook = malloc (sizeof (struct _perl_hook));
	hook->name = STRDUP (SvPV (ST (0), junk));
	hook->handler = STRDUP (SvPV (ST (1), junk));

	perl_hooks = list_append (perl_hooks, hook);

	TRACE (("registered %s (%s)\n", hook->name, hook->handler));

	XSRETURN_EMPTY;
}

#endif /* USE_PERL */
