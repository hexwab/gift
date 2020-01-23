/*
 * menu-if.h
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

#ifndef __MENU_IF_H
#define __MENU_IF_H

/**************************************************************************/

#ifndef ITEMFACTORY_SUCKS

static GtkItemFactoryEntry menu_if[] =
{
	{ "/_File",                 NULL,         NULL,           0, "<Branch>" },
	{ "/File/_Exit",            "<control>Q", gtk_main_quit,  0, NULL       },
	{ "/_Settings",             NULL,         NULL,           0, "<Branch>" },
	{ "/Settings/_General...",  NULL,         menu_pref_show, 0, NULL       },
};

#endif /* ITEMFACTORY_SUCKS */

/**************************************************************************/

#endif /* __MENU_IF_H */
