/*
 * perlc.h
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

#ifndef __GIFT_PERL_H
#define __GIFT_PERL_H

/*****************************************************************************/

#ifdef USE_PERL
void     perl_init ();
void     perl_autoload ();
void     perl_load_file (char *);
void     perl_cleanup ();
HookVar *perl_hook_event (char *name, char *args);
#endif /* USE_PERL */

/*****************************************************************************/

#endif /* __GIFT_PERL_H */
