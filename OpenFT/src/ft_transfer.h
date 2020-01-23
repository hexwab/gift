/*
 * $Id: ft_transfer.h,v 1.4 2003/07/25 09:31:01 jasta Exp $
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

#ifndef __FT_TRANSFER_H
#define __FT_TRANSFER_H

/*****************************************************************************/

/**
 * @file ft_transfer.h
 *
 * @brief Interfaces the OpenFT transfer system with giFT.
 */

/*****************************************************************************/

/**
 * Structure passed around in OpenFT space to unify all the components
 * necessary to satisfy a transfer.  This structure is used by downloads and
 * uploads alike, but in obviously different ways.  See ft_http_client.c or
 * ft_http_server.c to find out more.
 */
typedef struct
{
	TransferType   dir;                /**< Transfer direction */
	FILE          *f;                  /**< Opened FILE handle for
	                                    *   uploads only */
	TCPC          *http;               /**< HTTP connection used for
	                                    *   both directions */
	DatasetNode   *push_node;          /**< Download requests that need to
	                                    *   wait for an incoming PUSH will
	                                    *   assign this to the placement
	                                    *   in the internal push dataset */
#if 0
	BOOL           want_pong;          /**< Meaningless debugging trick */
#endif

	/**
	 * @name giFT Parameters
	 */
	Transfer      *transfer;
	Chunk         *chunk;
	Source        *source;
} FTTransfer;

/*****************************************************************************/

/**
 * Unserialized copy of the source URL passed back from search replies.
 */
struct ft_source
{
	in_addr_t host;                    /**< User who owns the file */
	in_port_t port;                    /**< HTTP port for that user */

	in_addr_t search_host;             /**< Search parent that replied with
	                                    *   the result that eventually produced
	                                    *   this source (if applicable) */
	in_port_t search_port;             /**< OpenFT port of that search node */

	char     *request;                 /**< url-encoded HTTP request (points
	                                    *   into ft_source::url) */
};

/*****************************************************************************/

/**
 * Construct a new FTTransfer object.  No funky implementation tricks here,
 * just simply allocation and assignment on the object.  I promise.
 */
FTTransfer *ft_transfer_new (TransferType dir,
                             Transfer *t, Chunk *c, Source *s);

/**
 * Cancel a transfer from plugin space.  That is, this routine will be used
 * when OpenFT wishes to signal cancellation to giFT, which will in turn call
 * ::openft_download_stop for us.  This method does not directly free the
 * xfer object, but giFT will be expected to deliver ::openft_download_stop
 * in the same call stack, so beware.
 */
void ft_transfer_stop (FTTransfer *xfer);

/*****************************************************************************/

void ft_transfer_set_transfer (FTTransfer *xfer, Transfer *t);
Transfer *ft_transfer_get_transfer (FTTransfer *xfer);

void ft_transfer_set_chunk (FTTransfer *xfer, Chunk *chunk);
Chunk *ft_transfer_get_chunk (FTTransfer *xfer);

void ft_transfer_set_source (FTTransfer *xfer, Source *source);
Source *ft_transfer_get_source (FTTransfer *xfer);

void ft_transfer_set_fhandle (FTTransfer *xfer, FILE *f);
FILE *ft_transfer_get_fhandle (FTTransfer *xfer);

/*****************************************************************************/

/**
 * Wrapper around the call Protocol::source_status for ease of use.
 */
void ft_transfer_status (FTTransfer *xfer, SourceStatus status,
                         const char *text);

/**
 * Convenience function which calls ::ft_transfer_status then
 * ::ft_transfer_stop.  This is used internally to report socket errors so
 * that they will show up on the interface protocol instead of having to
 * resort to using the log file.
 */
void ft_transfer_stop_status (FTTransfer *xfer, SourceStatus status,
                              const char *text);

/*****************************************************************************/

/**
 * Protocol method to begin a new download for giFT.
 */
BOOL openft_download_start (Protocol *p, Transfer *t, Chunk *c, Source *s);

/**
 * Protocol method to cancel a previously started download with
 * ::openft_download_start.  giFT should call this each time it wishes to
 * switch source/chunk as well, so it may be called multiple times per
 * Transfer object.
 */
void openft_download_stop (Protocol *p, Transfer *t, Chunk *c, Source *s,
                           BOOL complete);

/*****************************************************************************/

/**
 * Protocol method to cancel an upload.  This is only used when a user
 * requests the cancellation and is given so that we can free the FTTransfer
 * object associated with `c'.
 */
void openft_upload_stop (Protocol *p, Transfer *t, Chunk *c, Source *s);

/*****************************************************************************/

/**
 * Suspend the HTTP connection attached to this giFT transfer.
 */
BOOL openft_chunk_suspend (Protocol *p, Transfer *t, Chunk *c, Source *s);

/**
 * Resume event activity on the HTTP connection associated.
 */
BOOL openft_chunk_resume (Protocol *p, Transfer *t, Chunk *c, Source *s);

/*****************************************************************************/

/**
 * Protocol method raised when a new source is going to be added to the
 * transfer.  OpenFT uses this to parse the source url and attach that data
 * to the source object.  This data is accessed throughout the rest of the
 * internals and will be guaranteed until a successful call to
 * ::openft_source_remove.
 */
BOOL openft_source_add (Protocol *p, Transfer *t, Source *s);

/**
 * Destroy the parsed data allocated by ::openft_source_add.
 */
void openft_source_remove (Protocol *p, Transfer *t, Source *s);

/*****************************************************************************/

/**
 * Compare two sources to see if they are considered to be the same
 * host/share.  OpenFT uses host, hash, and request uri to determine
 * uniqueness.
 */
int openft_source_cmp (Protocol *p, Source *a, Source *b);

/**
 * Compare two users.  OpenFT will move past the alias (nodealias at ipaddr)
 * for comparison.
 */
int openft_user_cmp (Protocol *p, const char *a, const char *b);

/*****************************************************************************/

/**
 * Notify our parent search nodes that we have had an availability change so
 * that they may properly report this on any new searches in the future.  We
 * will also cache this value for ::ft_upload_avail.
 */
void openft_upload_avail (Protocol *p, unsigned long avail);

/*****************************************************************************/

/**
 * This is used only by ft_http_server.c to lookup the originally constructed
 * FTTransfer object when receiving a PUSH command from the uploading node.
 * After accessing the object, the entry will be removed from the table.
 */
FTTransfer *push_access (in_addr_t host, const char *request);

/**
 * Create a newly allocated array of all FTTransfer objects (which are then
 * referenced multiple times).  This is only used in proto/ft_push.c for the
 * purpose of removing definitely dead firewalled sources.
 */
Array *ft_downloads_access (void);

/*****************************************************************************/

/**
 * Access the current upload availability according to giFT.  This function
 * is implemented by monitoring p->upload_avail changes and caching each
 * result.
 */
unsigned long ft_upload_avail (void);

/*****************************************************************************/

#endif /* __FT_TRANSFER_H */
