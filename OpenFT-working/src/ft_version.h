/*
 * $Id: gt_version.h,v 1.7 2003/06/30 08:49:20 jasta Exp $
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

#if 0
#define COMPAT_MASK (ft_version (0xff, 0xff, 0x00, 0x00))
#define FT_VERSION_GT(a,b) ((a & COMPAT_MASK) > (b & COMPAT_MASK))
#define FT_VERSION_LT(a,b) ((a & COMPAT_MASK) < (b & COMPAT_MASK))
#define FT_VERSION_EQ(a,b) ((a & COMPAT_MASK) == (b & COMPAT_MASK))
#define FT_VERSION_CMP(a,b) ((a & COMPAT_MASK) - (b & COMPAT_MASK))
#endif

#define FT_VERSION_GT(a,b)   ft_version_gt(a,b)
#define FT_VERSION_LT(a,b)   ft_version_lt(a,b)
#define FT_VERSION_EQ(a,b)   ft_version_eq(a,b)
#define FT_VERSION_CMP(a,b)  \
	((ft_version_eq(a,b)) ? (0) : ((ft_version_gt(a,b)) ? (1) : (-1)))

#define FT_VERSION_LOCAL     ft_version_local()

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

BOOL ft_version_gt (ft_version_t a, ft_version_t b);
BOOL ft_version_lt (ft_version_t a, ft_version_t b);
BOOL ft_version_eq (ft_version_t a, ft_version_t b);

/*****************************************************************************/

#endif /* __FT_VERSION */
