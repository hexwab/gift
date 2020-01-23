/*
 * loader.c
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

#include "config.h"

#ifdef WIN32

/* to make the compiler STFU with _WALL */
int gift_main (int argc, char **argv);

/* for windows only: a loader to pass all args to the giFT.dll library */
int main (int argc, char **argv)
{
	return gift_main (argc, argv);
}

#endif /* WIN32 */
