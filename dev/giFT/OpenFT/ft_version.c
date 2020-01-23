/*
 * ft_version.c
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

#include "ft_openft.h"

#include "ft_version.h"

/*****************************************************************************/

static FTVersion local_ver = 0;

#define SHIFT(x,y) (((x) & 0xff) << (y))
#define UNSHIFT(v,x,y) if (x) *x = (((v) >> (y)) & 0xff)

/*****************************************************************************/

FTVersion ft_version (ft_uint8 major, ft_uint8 minor,
                      ft_uint8 micro, ft_uint8 rev)
{
	FTVersion ver;

	ver = SHIFT(major, 24) | SHIFT(minor, 16) | SHIFT(micro, 8) | SHIFT(rev, 0);

	return ver;
}

FTVersion ft_version_local ()
{
	if (local_ver)
		return local_ver;

	local_ver =
	    ft_version (OPENFT_MAJOR, OPENFT_MINOR, OPENFT_MICRO, OPENFT_REV);
	return local_ver;
}

void ft_version_parse (FTVersion version, ft_uint8 *major, ft_uint8 *minor,
                       ft_uint8 *micro, ft_uint8 *rev)
{
	UNSHIFT(version, major, 24);
	UNSHIFT(version, minor, 16);
	UNSHIFT(version, micro, 8);
	UNSHIFT(version, rev, 0);
}

/*****************************************************************************/

#if 0
int main ()
{
	FTVersion v;
	ft_uint8 mjr, mic, rev;

	v = ft_version (0, 0, 6, 10);

	if (FT_VERSION_GT(v, FT_VERSION_LOCAL)) printf ("GT\n");
	if (FT_VERSION_LT(v, FT_VERSION_LOCAL)) printf ("LT\n");
	if (FT_VERSION_EQ(v, FT_VERSION_LOCAL)) printf ("EQ\n");

	printf ("CMP=%i\n", FT_VERSION_CMP(v, FT_VERSION_LOCAL));

	ft_version_parse (v, &mjr, NULL, &mic, &rev);

	printf ("%i.%i.%i-%i\n", (int)mjr, 0, (int)mic, (int)rev);

	return 0;
}
#endif
