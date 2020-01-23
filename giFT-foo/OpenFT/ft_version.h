/*
 * $Id: ft_version.h,v 1.6 2003/06/01 07:13:48 jasta Exp $
 *
 * Copyright (C) 2001-2003 giFT project (gift.sourceforge.net)
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
typedef uint32_t ft_version_t;

/*****************************************************************************/

/**
 * Construct the version.
 */
ft_version_t ft_version (uint8_t major, uint8_t minor,
                         uint8_t micro, uint8_t rev);

/**
 * Parse the version.
 */
void ft_version_parse (ft_version_t version,
                       uint8_t *major, uint8_t *minor,
                       uint8_t *micro, uint8_t *rev);

/**
 * Return the local version.
 */
ft_version_t ft_version_local ();

/*****************************************************************************/

#endif /* __FT_VERSION */
