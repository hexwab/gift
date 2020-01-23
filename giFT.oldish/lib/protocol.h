/*
 * protocol.h
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

#ifndef __PROTOCOL_H
#define __PROTOCOL_H

/*****************************************************************************/

/**
 * @file protocol.h
 *
 * \todo this
 */

/*****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif /* __reallycrappylanguage */

/*****************************************************************************/

struct _transfer;
struct _chunk;
struct _source;
struct _if_event;
struct _file_share;

#define Transfer struct _transfer
#define Chunk struct _chunk
#define Source struct _source
#define IFEvent struct _if_event
#define FileShare struct _file_share

/*****************************************************************************/

/**
 * Protocol structure containing methods for bidirectional communication
 * between daemon and protocol plugin.
 */
typedef struct _protocol
{
	/*************************************************************************/
	/**
	 * @name Public
	 * Public members.
	 */

	char     *name;                    /**< protocol name */
	void     *udata;                   /**< arbitrary data associated by the
	                                    *   "plugin" */

	/**
	 * @name Private
	 * Private members.
	 */

	void *handle;                      /**< dlopen handle */

	/**
	 * protocol features
	 *
	 * @param range-get
	 * @param user-browse
	 * @param hash-unique
	 * @param chat-user
	 * @param chat-group
	 */
	Dataset *features;

	/*************************************************************************/
	/**
	 * @name giFT Assigned Methods
	 *
	 * Communication methods defined by giFT before protocol's
	 * initialization.  No protocol should override these, however, most
	 * protocols will want to call at least some of these throughout the
	 * course of their protocol's lifespan.
	 */

	/**
	 * Initialization function.  This should not actually "start" the
	 * protocol, merely setup the Protocol structure passed.  Note that giFT
	 * assigns this value before plugin initialization is called, and will
	 * always be set to your protocol's fully qualified case-sensitive name
	 * with the string literal "_init" appened to the ending.  So, for
	 * OpenFT's case, this value will be set to OpenFT_init prior to the
	 * calling of said function.
	 *
	 * @return Boolean success or failure.
	 */
	int (*init) (struct _protocol *p);

	/**
	 * Set this protocols hashing algorithms.  You may set more than one
	 * algorithm with this function.
	 *
	 * @param p
	 * @param type  Hashing alogrithm name (ie MD5).
	 * @param algo  Function pointer to main hashing routine.
	 * @param human Function pointer to the routine which will produce a
	 *              "human-readable" hash given the return value from
	 *              \em algo.
	 *
	 * @return If the supplied algo has already been registered or cannot
	 *         register, FALSE is returned.  Otherwise, TRUE.
	 */
	int (*hash_set) (struct _protocol *p, char *type,
	                 unsigned char* (*algo) (char *path, char *type, int *len),
	                 char* (*human) (unsigned char *hash, int *len));

	/**
	 * Add a protocol feature.  This informs giFT of the various
	 * protocol/network-specific features this plugin can or can not
	 * successfully implement so as to modify giFT's behaviour.  Currently,
	 * none of the feature-set defined by the default plugin (OpenFT) is
	 * actually used conditionally, however there will come a time when that
	 * is the case.  For now it is probably safe just to ignore this. For a
	 * detailed list of supported options, see Protocol::features.
	 *
	 * @param p
	 * @param feature  Feature's name, this is not an arbitrary string and
	 *                 should be selected from a determined list that we
	 *                 will eventually provide.
	 * @param spt      Boolean value for this feature's support.
	 */
	void (*support) (struct _protocol *p, char *feature, int spt);

	/**
	 * Write data to the specified bidirectional chunk object.  This is used
	 * for transfers when data has either been completely received or sent
	 * out (not considering kernel-level socket buffering).  giFT will
	 * internally track transfer progress and/or write the necessary data to
	 * file.
	 *
	 * @param p
	 * @param transfer
	 * @param chunk
	 * @param source
	 * @param data
	 * @param len
	 */
	void (*chunk_write) (struct _protocol *p,
	                     Transfer *transfer, Chunk *chunk, Source *source,
	                     unsigned char *data, size_t len);

	/**
	 * Set the source status.
	 *
	 * @param p
	 * @param source
	 * @param klass   Basic status grouping so that giFT may still apply
	 *                some broad logic based on transfer progress/status.
	 * @param disptxt Literal text to display to the giFT clients.
	 */
	void (*source_status) (struct _protocol *p, Source *source,
	                       unsigned short klass, char *disptxt);

	/**
	 * Send a search result from the plugin.  This communication is used by
	 * the protocol when results need to be delivered to the interface
	 * protocol.
	 *
	 * @param p
	 * @param event Internal event delivered by the search request.
	 * @param user  Arbitrary protocol-specific string attempting to uniquely
	 *              represent the user which has the result.
	 * @param node  Parent node or server which returned the result.  May be
	 *              NULL.
	 * @param href  Protocol-specific reference string.  Example:
	 *              Napster://jasta[gnapster]@NecessaryEvil/file.mp3
	 * @param avail Availability of this file.  If the protocol has no concept
	 *              of this, use 0.  You should also inform giFT that your
	 *              protocol has no support for availability through
	 *              Protocol::support.
	 * @param file  Complete FileShare structure.  See Protocol::share_new.
	 */
	void (*search_result) (struct _protocol *p, IFEvent *event,
	                       char *user, char *node, char *href,
	                       unsigned int avail, FileShare *file);

	/**
	 * Inform giFT that all search results that can be delivered have been
	 * delivered, or there is some reasonable reason to assume we have
	 * received the bulk of all results (if your protocol has no concept of
	 * clean search termination).  This will "cancel" the search event
	 * gracefully and return to the interface protocol that your plugin has
	 * terminated.
	 */
	void (*search_complete) (struct _protocol *p, IFEvent *event);

	/**
	 * Deliver a raw message to the interface protocol.
	 *
	 * @param p
	 * @param message
	 * @param persist If true, this message will be delivered each time a user
	 *                attaches to the running daemon.
	 */
	void (*message) (struct _protocol *p, char *message);

	/*************************************************************************/
	/**
	 * @name Protocol-specified Communication
	 *
	 * These methods will be defined by your protocol at the time of
	 * Protocol::init. You should understand that you do not need to
	 * initialize all these values, as giFT will provide dummy
	 * implementations for them as to guarantee no method points to NULL.
	 * However, you should note that a failure to set certain key
	 * communication methods will result in a nearly worthless protocol
	 * plugin.
	 */

	/**
	 * Start the protocol connections.  See Protocol::init for a more
	 * detailed description as to why this separation was decided upon.
	 *
	 * @return Boolean success or failure.
	 */
	int (*start) (struct _protocol *p);

	/**
	 * Cleanup everything you have initialized/allocated.  This will
	 * generally mean that your protocol is being unloaded either by a giFT
	 * shutdown or by a simple unload directive to this specific protocol.
	 */
	void (*destroy) (struct _protocol *p);

	/**
	 * Begin a new chunk transfer.  Note that the protocol is not allowed to
	 * differentiate between multi-sourced and single-sourced transfers, you
	 * must simply obey what is specified by the \em chunk argument.
	 *
	 * @param p
	 * @param transfer
	 * @param chunk
	 * @param source
	 *
	 * @return Boolean success or failure.
	 */
	int (*download_start) (struct _protocol *p,
	                       Transfer *transfer, Chunk *chunk, Source *source);

	/**
	 * Cancel an active download.  Protocols are expected to cleanup all
	 * sockets and internal data associated with this chunk.  Please note
	 * that OpenFT violates this specification by attempting to use
	 * persisting HTTP connections for transfers.
	 *
	 * @param p
	 * @param transfer
	 * @param chunk
	 * @param source
	 * @param complete Indicates whether or not this chunk completed
	 *                 successfully.  Do not attempt to figure this out
	 *                 on your own using chunk->start/stop as this flag
	 *                 has other considerations.
	 */
	void (*download_stop) (struct _protocol *p,
	                       Transfer *transfer, Chunk *chunk, Source *source,
	                       int complete);

	/**
	 * See Protocol::download_start.
	 */
	int (*upload_start) (struct _protocol *p,
	                     Transfer *transfer, Chunk *chunk, Source *source,
	                     unsigned long avail);

	/**
	 * See Protocol::download_stop.
	 */
	void (*upload_stop) (struct _protocol *p,
	                     Transfer *transfer, Chunk *chunk, Source *source,
	                     unsigned long avail);


	/**
	 * Suspend input/output on the specified chunk until resumed later.  giFT
	 * uses these methods for managing transfer bandwidth usage (user-space
	 * throttling).  Protocols should do their best to implement this.
	 *
	 * @param p
	 * @param transfer
	 * @param chunk
	 * @param source
	 *
	 * @return Boolean success or failure.  Return FALSE if your protocol
	 *         does not support this feature.
	 */
	int (*chunk_suspend) (struct _protocol *p,
	                      Transfer *transfer, Chunk *chunk, Source *source);

	/**
	 * Resume a suspended transfer.  See Protocol::chunk_suspend.
	 */
	int (*chunk_resume) (struct _protocol *p,
	                     Transfer *transfer, Chunk *chunk, Source *source);

	/**
	 * Remove all activity and pending data for the specified source.  This
	 * method is used when giFT has been instructed to remove a source from
	 * the specified transfer (you may assume that this is a download).
	 */
	int (*source_remove) (struct _protocol *p,
	                      Transfer *transfer, Source *source);

	/**
	 * Search request.  Please note that Protocol::browse and
	 * Protocol::locate use the same result and completion routines as they
	 * are actually the same in giFT space.
	 *
	 * @param p
	 * @param event   Internal event used for search response.
	 * @param query   Query string.
	 * @param exclude Exclude string.
	 * @param realm   Filtering by group.
	 * @param meta    Extended search information.
	 *
	 * @return If true, the search will be assumed to have succeeded pending
	 *         a response from the protocol at some later time.  Otherwise, the
	 *         search is terminated immediately afterwards and anymore
	 *         replies to this event will be considered invalid.
	 */
	int (*search) (struct _protocol *p,
	               IFEvent *event, char *query, char *exclude,
	               char *realm, Dataset *meta);

	/**
	 * Browse an individual users content.  The response to this event uses
	 * the same systems as a search.
	 *
	 * @param p
	 * @param event
	 * @param user  User to browse.  You should expect this in the form you
	 *              return from search results.
	 * @param node  Node/server that reported this users shares, if available.
	 *              If possible, do not rely on this value as it is only
	 *              provided for search results.
	 */
	int (*browse) (struct _protocol *p,
	               IFEvent *event, char *user, char *node);

	/**
	 * Locate a file on the network by some unique content.  See
	 * Protocol::browse and Protocol::search.
	 *
	 * @param p
	 * @param event
	 * @param hash
	 */
	int (*locate) (struct _protocol *p,
	               IFEvent *event, char *hash);

	/**
	 * Cancel a previously requested search.
	 *
	 * @param p
	 * @param event Internal event delivered by the search request.
	 */
	void (*search_cancel) (struct _protocol *p, IFEvent *event);

	/**
	 * Allocate new protocol-specific data to be associated with \em file.
	 *
	 * @return Pointer to protocol-specific dynamically allocated memory.
	 */
	void* (*share_new) (struct _protocol *p, FileShare *file);

	/**
	 * Destroy data returned by Protocol::share_new.
	 */
	void (*share_free) (struct _protocol *p, FileShare *file, void *data);

	/**
	 * Add a new locally shared file to the network.
	 *
	 * @param p
	 * @param file
	 * @param data  Arbitrary data returned from Protocol::share_new.
	 *
	 * @return Boolean success or failure.
	 */
	int (*share_add) (struct _protocol *p, FileShare *file, void *data);

	/**
	 * Remove a locally shared file from the network.  See Protocol::share_add.
	 */
	int (*share_remove) (struct _protocol *p, FileShare *file, void *data);

	/**
	 * giFT has begun syncing shares.  Expect Protocol::share_add and
	 * Protocol::share_remove calls until \em begin is FALSE, in which case
	 * the sync has completed.  This is to be considered an effective
	 * modification of the current local shares and should be sent out over
	 * the socket if said protocol requires this.
	 *
	 * @param p
	 * @param begin
	 */
	void (*share_sync) (struct _protocol *p, int begin);

	/**
	 * Temporarily unshare all files.  Some protocols (eg OpenFT) implement
	 * this with a single command disabling shares instead of removing them.
	 * To my knowledge, most other protocols will require you actually
	 * unshare every file manually.
	 */
	void (*share_hide) (struct _protocol *p);

	/**
	 * Enable sharing after it was previously hidden.
	 */
	void (*share_show) (struct _protocol *p, unsigned long avail);

	/**
	 * Request protocol statistics.
	 *
	 * @param p
	 * @param users  Storage location for the number of users.
	 * @param files  Storage location for the number of files.
	 * @param size   Storage location for the total shared size (GB).
	 * @param extra  Extra information this protocol would like to be
	 *               reported.  Please use sparingly.
	 *
	 * @return Number of connections currently established.  Any value above
	 *         zero will be considered "Online".
	 */
	int (*stats) (struct _protocol *p,
	              unsigned long *users, unsigned long *files,
	              double *size, Dataset **extra);

	/*************************************************************************/
	/**
	 * @name Optional Protocol Communication
	 *
	 * Methods which can (and probably should) be specified by each protocol,
	 * but will have sane and useful default implementations specified by
	 * giFT.  Your protocol will not be adversely affected if you leave these
	 * values as is.
	 */

	/**
	 * Compare two sources.  This will be used when selecting how to manage
	 * multiple "similar" sources to a single transfer.  The sources should
	 * compare equal (return value of 0) when it can be determined that they
	 * are the same user sharing the same file.
	 *
	 * @return Identical to strcmp.
	 */
	int (*source_cmp) (struct _protocol *p, Source *a, Source *b);

	/**
	 * Compare two users.  This was specially added for OpenFT so that node
	 * aliases could be weeded out as a significant portion of the username
	 * field.  Default implementation here is a simple strcmp.
	 */
	int (*user_cmp) (struct _protocol *p, char *a, char *b);
} Protocol;

/*****************************************************************************/

#undef Transfer
#undef Chunk
#undef Source
#undef IFEvent
#undef FileShare

/*****************************************************************************/

/**
 * Create a new protocol structure.  This completely initializes the function
 * pointer table with dummy functions expecting that the caller (giFT) fill
 * in the _actual_ implementations.  Also adds to the list of active
 * protocols.
 *
 * @param name Protocol name for lookup purposes.
 */
Protocol *protocol_new (char *name);

/**
 * Destroy the supplied protocol structure.  This handles removal from the
 * protocol list as well.
 */
void protocol_free (Protocol *p);

/**
 * Retrieve the protocol structure given the string literal name.
 */
Protocol *protocol_lookup (char *name);

/**
 * Loop through all 'active' protocols.  See Protocol::dataset_foreach_ex.
 *
 * @param func
 * @param udata
 */
void protocol_foreach (DatasetForeach func, void *udata);

/**
 * Same as Protocol::protocol_foreach except that each protocol will be
 * unloaded after the call to \em func.
 */
void protocol_foreachclear (DatasetForeach func, void *udata);

/*****************************************************************************/

#ifdef __cplusplus
}
#endif /* __reallycrappylanguage */

/*****************************************************************************/

#endif /* __PROTOCOL_H */
