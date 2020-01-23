/*
 * $Id: ft_share_file.h,v 1.10 2003/05/26 11:47:40 jasta Exp $
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

#ifndef __FT_SHARE_FILE_H
#define __FT_SHARE_FILE_H

/*****************************************************************************/

/**
 * @file ft_share_file.h
 */

/*****************************************************************************/

#include "plugin/share.h"

#if 0
#include "src/share_cache.h"
#endif

#include "ft_shost.h"

/*****************************************************************************/

/**
 * Arbitrary OpenFT data associated with giFT space FileShare objects.  This
 * structure is used for both local giFT shares and shares OpenFT creates
 * still utilizing the FileShare structure.
 */
typedef struct
{
	FTSHost  *shost;                   /**< reference the share host parent
	                                    *   that owns this file.  for local
	                                    *   shares, this is initialized from
	                                    *   FT_SELF. */
	uint32_t *tokens;                  /**< list of searchable tokens for
	                                    *   for this file. */
} FTShare;

/*****************************************************************************/

/**
 * Allocate a new FileShare structure for use solely in OpenFT space.  This
 * actually calls ::share_new from giFT space, but ideally it should be
 * constructing the FileShare object itself as this violates the new
 * communication model.
 *
 * @param shost    Parent share host object that owns this file.
 * @param size     Total size of file.
 * @param md5      16 byte MD5 sum which represents this file.
 * @param mime     MIME type.
 * @param filename Fully qualified path with filename.
 */
Share *ft_share_new (FTSHost *shost,
                     off_t size, unsigned char *md5,
                     char *mime, char *filename);

/**
 * Free a previously allocated FileShare structure (from ::ft_share_new).
 * This should not be used to modify FileShare structures legitimately
 * allocated from within giFT space.
 */
void ft_share_free (Share *file);

/**
 * Create arbitrary OpenFT-specific data to be associated w/ a FileShare
 * structure (regardless of which realm allocated the structure).  This
 * is used through the protocol communication mechanism as well as from
 * ::ft_share_new.  It does not actually link the data (through
 * ::share_insert_data) to \em file.
 *
 * @return Pointer to dynamically allocated memory which is expected to be
 *         somehow linked to \em file.
 */
FTShare *ft_share_new_data (Share *file, FTSHost *shost);

/**
 * Free arbitrary OpenFT-specific data allocated from ::ft_share_new_data.
 * Similarly to ::ft_share_new_data, this will be called from the protocol
 * structure as well from ::ft_share_free.
 */
void ft_share_free_data (Share *file, FTShare *share);

/*****************************************************************************/

/**
 * Increment the reference (mortality count) for this file.  No FileShare
 * will be able to be unallocated until it's reference reaches 0, in which
 * case OpenFT can be certain the structure is not in use via any other
 * subsystem.
 *
 * @return Reference value after incrementation has occurred.
 */
unsigned int ft_share_ref (Share *file);

/**
 * Decrement the reference for the supplied file.  Please note that this
 * function will automatically free any file whose references count reaches
 * 0 (or was already 0, by some freakish accident) by this decrementation.  If
 * this function returns 0, you should assume subsequent accesses to \em file
 * are illegal.
 *
 * @return Reference value after decrementation has occurred.
 */
unsigned int ft_share_unref (Share *file);

/*****************************************************************************/

/**
 * Verify a FileShare object (allocated from OpenFT space) for completion.
 * That is, ensure that all required fields exist and have sane values.
 *
 * @return If true, the object is considered usable.  Otherwise, it is in
 *         some way incomplete and should not be considered qualified.
 */
BOOL ft_share_complete (Share *file);

/*****************************************************************************/

#endif /* __FT_SHARE_FILE_H */
