/*
 * ft_version.h
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

#ifndef __FT_VERSION
#define __FT_VERSION

/*****************************************************************************/

/**
 * @file ft_version.h
 *
 * @brief OpenFT versioning support.
 */

/*****************************************************************************/

#define COMPAT_MASK (ft_version (0xff, 0xff, 0xff, 0x00))
#define FT_VERSION_GT(a,b) ((a & COMPAT_MASK) > (b & COMPAT_MASK))
#define FT_VERSION_LT(a,b) ((a & COMPAT_MASK) < (b & COMPAT_MASK))
#define FT_VERSION_EQ(a,b) ((a & COMPAT_MASK) == (b & COMPAT_MASK))
#define FT_VERSION_CMP(a,b) ((a & COMPAT_MASK) - (b & COMPAT_MASK))
#define FT_VERSION_LOCAL ft_version_local()

/**
 * Type large enough to hold the versioning information.
 */
typedef ft_uint32 FTVersion;

/*****************************************************************************/

/**
 * Construct the version.
 */
FTVersion ft_version (ft_uint8 major, ft_uint8 minor,
                      ft_uint8 micro, ft_uint8 rev);

/**
 * Parse the version.
 */
void ft_version_parse (FTVersion version, ft_uint8 *major, ft_uint8 *minor,
                       ft_uint8 *micro, ft_uint8 *rev);

/**
 * Return the local version.
 */
FTVersion ft_version_local ();

/*****************************************************************************/

#endif /* __FT_VERSION */
