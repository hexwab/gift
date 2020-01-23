/*
 * hook.h
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

#ifndef __HOOK_H
#define __HOOK_H

/*****************************************************************************/

typedef enum
{
	HOOK_VAR_NUL = 0,
	HOOK_VAR_INT,
	HOOK_VAR_STR
} HookVarType;

typedef struct
{
	HookVarType type;
	union
	{
		int    num;                    /* i wish this could be named int */
		double dbl;
		char  *str;
	} value;
} HookVar;

/*****************************************************************************/

HookVar *hook_event (char *name, int ret, ...);

/*****************************************************************************/

HookVar *hook_var_new   (HookVarType type, void *value);
void     hook_var_free  (HookVar *hv);

void    *hook_var_value (HookVar *hv);

/*****************************************************************************/

#endif /* __HOOK_H */
