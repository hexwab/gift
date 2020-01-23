/*
 * $Id: protocol.h,v 1.20 2003/06/27 10:49:40 jasta Exp $
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

#ifndef __PROTOCOL_H
#define __PROTOCOL_H

/*****************************************************************************/

/**
 * @file protocol.h
 *
 * \todo This.
 */

/*****************************************************************************/

#ifdef __cplusplus
extern "C"
{
#endif /* __reallycrappylanguage */

/*****************************************************************************/

struct protocol;
struct if_event;
struct file_share;

#include "transfer_api.h"
#include "if_event_api.h"

#include "share_hash.h"

/*****************************************************************************/

#define LIBGIFTPROTO_MAJOR  (0)
#define LIBGIFTPROTO_MINOR (10)
#define LIBGIFTPROTO_MICRO  (1)

#define LIBGIFTPROTO_MKVERSION(major,minor,micro) \
    ((((major) & 0xff) << 24) |                   \
     (((minor) & 0xff) << 16) |                   \
     (((micro) & 0xff) <<  8))

#define LIBGIFTPROTO_VERSION (LIBGIFTPROTO_MKVERSION(0,10,1))

#if 0
#define LIBGIFTPROTO_VERSION                   \
    LIBGIFTPROTO_MKVERSION(LIBGIFTPROTO_MAJOR, \
                           LIBGIFTPROTO_MINOR, \
                           LIBGIFTPROTO_MICRO)
#endif

/*****************************************************************************/

/**
 * Protocol structure containing methods for bidirectional communication
 * between daemon and protocol plugin.
 */
typedef struct protocol
{
	/*************************************************************************/
	/**
	 * @name Public
	 * Public members.
	 */

	char     *name;                    /**< protocol name */
	void     *udata;                   /**< arbitrary data associated by the
	                                    *   plugin */

	/**
	 * @name Private
	 * Private members.
	 */

	void     *handle;                  /**< lt_dlopen handle (NOTE: we cant
	                                    *   use lt_dlhandle here because
	                                    *   plugins depend on the size of
	                                    *   this structure, but may not
	                                    *   have lt_dlopen available!) */

	Dataset  *hashes;                  /**< alternate index of hashes on
	                                    *   a per-protocol basis */

	/**
	 * Protocol features
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
	 * @param p        Protocol pointer associated with the plugin being
	 *                 initialized.  This object is a private communication
	 *                 stream between giFT and the plugin exclusively.
	 *
	 * @return Boolean success or failure.
	 */
	BOOL (*init) (struct protocol *p);

	/* simple shorthand for the macros below */
#define FILE_LINE_FUNC __FILE__,__LINE__,__PRETTY_FUNCTION__

#if defined (__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
# define DBGFN(p,...) trace(p,FILE_LINE_FUNC,__VA_ARGS__)
# define DBGSOCK(p,c,...) tracesock(p,c,FILE_LINE_FUNC,__VA_ARGS__)
#elif defined (__GNUC__)
# define DBGFN(p,fmt...) trace(p,FILE_LINE_FUNC,fmt)
# define DBGSOCK(p,c,fmt...) tracesock(p,c,FILE_LINE_FUNC,fmt)
#else
# define DBGFN dbg
# define DBGSOCK dbgsock
#endif

	/**
	 * Method for producing informative log lines from within a protocol
	 * plugin.  Do not use this function directly, see ::DBGFN instead.
	 */
	int (*trace) (struct protocol *p, char *file, int line, char *func,
	              char *fmt, ...);

	/**
	 * Special debugging method that will automatically format and display
	 * the peer information.  This would not be provided unless I was pretty
	 * sure every plugin will use remote sockets <g>.
	 */
	int (*tracesock) (struct protocol *p, TCPC *c,
					  char *file, int line, char *func, char *fmt, ...);

	/**
	 * Print debugging information to the appropriate logging channel.  This
	 * data does not include information about the line that produced the log
	 * message.
	 */
	int (*dbg) (struct protocol *p, char *fmt, ...);

	/**
	 * See Protocol::dbg and Protocol::tracesock.
	 */
	int (*dbgsock) (struct protocol *p, TCPC *c, char *fmt, ...);

	/**
	 * Logging facility using the warning log channel.
	 */
	int (*warn) (struct protocol *p, char *fmt, ...);

	/**
	 * Logging facility using the error log channel.
	 */
	int (*err) (struct protocol *p, char *fmt, ...);

#define HASH_PRIMARY   0x01
#define HASH_SECONDARY 0x02
#define HASH_LOCAL     0x04            /* HASH_PRIMARY implicitly adds this */

	/**
	 * Adds a new hashing algorithm handler for this plugin.  Please note
	 * that while only one primary hashing routine may be specified, multiple
	 * secondary routines are allowed.
	 *
	 * @param type    String literal to represent the hashing algorithm.
	 *                Common names are SHA1, MD5, etc.
	 * @param opt     Handler options specific to the way giFT understands what
	 *                your plugin needs.  Using HASH_PRIMARY will cause all
	 *                local shares to be hashed, however, HASH_SECONDARY algos
	 *                will still be able to verify download integrity.  If
	 *                the HASH_LOCAL flag is present, local shares will be
	 *                hashed using the supplied secondary algorithm.
	 * @param algofn
	 * @param dspfn   Callback handler used to create an ASCII string
	 *                representation of the hash supplied.  If NULL is
	 *                supplied, a simple hex conversion will take place.
	 *
	 * @return Boolean success or failure.
	 */
	BOOL (*hash_handler) (struct protocol *p, const char *type, int opt,
	                      HashFn algofn, HashDspFn dspfn);

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
	void (*support) (struct protocol *p, char *feature, BOOL spt);

#define SHARE_LOOKUP_HASH    (1)       /**< By stored hash */
#define SHARE_LOOKUP_HPATH   (2)       /**< By exclusion (hidden) path */
#define SHARE_LOOKUP_PATH    (3)       /**< Full local path */
	/**
	 * Lookup a locally shared file based on `command' criteria.  This is the
	 * only interface to directly query the internally managed share cache.
	 *
	 * \example
	 * share = p->share_lookup (p, SHARE_LOOKUP_HASH, "MD5", md5sum, 16);
	 * \endexample
	 *
	 * I apologize for the relative lack of documentation provided for this
	 * function.  Hopefully I will get around to it soon (boy do I say that a
	 * lot *grin*).
	 *
	 * @param p
	 * @param command  Lookup type to perform.  Possible commands are defined
	 *                 above.
	 * @param ...      Command-specific lookup arguments.
	 *
	 * @return Direct pointer into the local share cache or NULL if the
	 *         entry was not found.
	 */
	struct file_share* (*share_lookup) (struct protocol *p, int command, ...);

#define UPLOAD_AUTH_ALLOW          (0)
#define UPLOAD_AUTH_STALE         (-1)
#define UPLOAD_AUTH_MAX           (-2)
#define UPLOAD_AUTH_MAX_PERUSER   (-3)
#define UPLOAD_AUTH_NOTSHARED     (-4)
	/**
	 * Inform giFT of a new upload request, specifically one which needs
	 * access to a locally shared object (that is, something in
	 * gift:/sharing/root).  The return value from this function will help
	 * determine how the plugin should proceed.
	 *
	 * @param p
	 * @param user   Username that can be compared with previously
	 *               registered uploads.  Consider setting and using
	 *               Protocol::user_cmp for this to work well.
	 * @param share  Share that we are considering for upload.
	 *
	 * @retval UPLOAD_AUTH_ALLOW
	 *
	 *   Upload has been accepted.  You still need to call
	 *   Protocol::upload_start when the transfer should be displayed. Please
	 *   note that you must minimize the window between an upload_request
	 *   call and an upload_start call to prevent users from slipping under
	 *   the queue.
	 *
	 * @retval UPLOAD_AUTH_STALE
	 *
	 *   The share requested has changed since the last share sync.  This
	 *   triggers an immediate non-blocking share sync, and the transfer
	 *   should be temporarily aborted.
	 *
	 * @retval UPLOAD_AUTH_MAX
	 *
	 *   Maximum uploads currently reached.  Please note that an internally
	 *   managed fair upload queue is maintained and can be access using
	 *   Protocol::upload_queue.  This queue is based on the assumption that
	 *   the download will be retried until allowed.  You may disable this
	 *   functionality through the features dataset.
	 *
	 * @retval UPLOAD_AUTH_MAX_PERUSER
	 *
	 *   The user is already downloading a file from us, and will not be
	 *   allowed to occupy two positions at once.  A local queue should be
	 *   implemented in whichever client made this request.
	 *
	 * @retval UPLOAD_AUTH_NOTSHARED
	 *
	 *   The requested object is not a valid share according to the currently
	 * loaded shares database.  No action is taken to resync automatically.
	 */
	int (*upload_auth) (struct protocol *p, const char *user,
	                    struct file_share *share);

	/**
	 * Inform giFT of a new upload which is about to begin.  Please note that
	 * this does not tackle authorization of the share and in fact allows
	 * subversion of that entire system if you so wish.  For normal uploads
	 * you will definitely want to obey the auth rules so I highly suggest
	 * calling and properly handling Protocol::upload_request prior to using
	 * this method.
	 *
	 * @param p
	 * @param user   Username that will be reported to the interface protocol.
	 * @param share
	 *
	 *   Shared object used to specify the upload.  Under normal
	 * 	 circumstances this should be returned to you from
	 * 	 Protocol::share_lookup.  Please note that the hash value will be
	 * 	 extracted based on the registered hashing algorithms of this
	 * 	 protocol.
	 *
	 *   This interface does not require that you use a share from
	 *   Protocol::share_lookup, only that it is recommended.  You may
	 *   optionally provide your own protocol-defined upload by filling in
	 *   the following fields:
	 *
	 *   Share::path
	 *   Share::mime
	 *   Share::size
	 *
	 *   You must also set a valid hash for the primary registered hashing
	 *   algorithm for your plugin.
	 *
	 * @param start  Beginning file offset that will be uploaded.
	 * @param stop   End offset to be delivered.
	 *
	 * @return Transfer handle to use with the subsequent called to
	 *         Protocol::chunk_write.  You may at any time receive a
	 *         Protocol::upload_stop call that will request you clean up
	 *         any data you associate after the transfer has begun.
	 */
	struct transfer* (*upload_start) (struct protocol *p,
	                                  const char *user,
	                                  struct file_share *share,
	                                  off_t start, off_t stop);

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
	void (*chunk_write) (struct protocol *p,
	                     struct transfer *transfer,
	                     struct chunk *chunk, struct source *source,
	                     unsigned char *data, size_t len);

	/**
	 * Cancel a source as though the user had requested its removal.  This is
	 * useful when your plugin knows (much better than giFT) that attempting
	 * to retrieve from this source is futile or that it could be dangerous.
	 * Please note that giFT has an internal automagic timeout for sources
	 * that do not have data throughput.
	 *
	 * @param p
	 * @param transfer
	 * @param source
	 */
	void (*source_abort) (struct protocol *p,
						  struct transfer *transfer, struct source *source);

	/**
	 * Set the source status.
	 *
	 * @param p
	 * @param source
	 * @param klass   Basic status grouping so that giFT may still apply
	 *                some broad logic based on transfer progress/status.
	 * @param disptxt Literal text to display to the giFT clients.
	 */
	void (*source_status) (struct protocol *p, struct source *source,
	                       enum source_status klass, const char *disptxt);

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
	void (*search_result) (struct protocol *p, struct if_event *event,
	                       char *user, char *node, char *href,
	                       unsigned int avail, struct file_share *file);

	/**
	 * Inform giFT that all search results that can be delivered have been
	 * delivered, or there is some reasonable reason to assume we have
	 * received the bulk of all results (if your protocol has no concept of
	 * clean search termination).  This will "cancel" the search event
	 * gracefully and return to the interface protocol that your plugin has
	 * terminated.
	 */
	void (*search_complete) (struct protocol *p, struct if_event *event);

	/**
	 * Deliver a raw message to the interface protocol.
	 *
	 * @param p
	 * @param message
	 * @param persist If true, this message will be delivered each time a user
	 *                attaches to the running daemon.
	 */
	void (*message) (struct protocol *p, char *message);

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
	BOOL (*start) (struct protocol *p);

	/**
	 * Cleanup everything you have initialized/allocated.  This will
	 * generally mean that your protocol is being unloaded either by a giFT
	 * shutdown or by a simple unload directive to this specific protocol.
	 */
	void (*destroy) (struct protocol *p);

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
	BOOL (*download_start) (struct protocol *p,
	                        struct transfer *transfer,
	                        struct chunk *chunk, struct source *source);

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
	void (*download_stop) (struct protocol *p,
	                       struct transfer *transfer,
	                       struct chunk *chunk, struct source *source,
	                       int complete);

	/**
	 * See Protocol::download_stop.
	 */
	void (*upload_stop) (struct protocol *p,
	                     struct transfer *transfer,
	                     struct chunk *chunk, struct source *source);

	/**
	 * Notify the protocol of upload availability changes.  Protocols
	 * such as OpenFT take advantage of this by delivering to parent search
	 * nodes so that search results for other users can indicate the
	 * availability before download.  Any protocol that has no concept of
	 * upload eligibility/availability, can safely ignore this.
	 *
	 * @param p
	 * @param avail  Total number of available upload positions.
	 */
	void (*upload_avail) (struct protocol *p, unsigned long avail);

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
	BOOL (*chunk_suspend) (struct protocol *p,
	                       struct transfer *transfer,
	                       struct chunk *chunk, struct source *source);

	/**
	 * Resume a suspended transfer.  See Protocol::chunk_suspend.
	 */
	BOOL (*chunk_resume) (struct protocol *p,
	                      struct transfer *transfer,
	                      struct chunk *chunk, struct source *source);

	/**
	 * Remove all activity and pending data for the specified source.  This
	 * method is used when giFT has been instructed to remove a source from
	 * the specified transfer (you may assume that this is a download).
	 */
	int (*source_remove) (struct protocol *p,
	                      struct transfer *transfer, struct source *source);

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
	int (*search) (struct protocol *p,
	               struct if_event *event, char *query, char *exclude,
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
	int (*browse) (struct protocol *p,
	               struct if_event *event, char *user, char *node);

	/**
	 * Locate a file on the network by some unique content.  See
	 * Protocol::browse and Protocol::search.
	 *
	 * @param p
	 * @param event
	 * @param htype  Optional hashing algorithm name.  If NULL, assume
	 *               that the hashing type is whatever you registered using
	 *               HASH_PRIMARY.
	 * @param hash   NUL-terminated ASCII string representing the hash to
	 *               search for.
	 */
	int (*locate) (struct protocol *p,
	               struct if_event *event, char *htype, char *hash);

	/**
	 * Cancel a previously requested search.
	 *
	 * @param p
	 * @param event Internal event delivered by the search request.
	 */
	void (*search_cancel) (struct protocol *p, struct if_event *event);

	/**
	 * Allocate new protocol-specific data to be associated with \em file.
	 *
	 * @return Pointer to protocol-specific dynamically allocated memory.
	 */
	void* (*share_new) (struct protocol *p, struct file_share *file);

	/**
	 * Destroy data returned by Protocol::share_new.
	 */
	void (*share_free) (struct protocol *p, struct file_share *file,
	                    void *data);

	/**
	 * Add a new locally shared file to the network.
	 *
	 * @param p
	 * @param file
	 * @param data  Arbitrary data returned from Protocol::share_new.
	 *
	 * @return Boolean success or failure.
	 */
	BOOL (*share_add) (struct protocol *p, struct file_share *file, void *data);

	/**
	 * Remove a locally shared file from the network.  See Protocol::share_add.
	 */
	BOOL (*share_remove) (struct protocol *p, struct file_share *file,
	                      void *data);

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
	void (*share_sync) (struct protocol *p, int begin);

	/**
	 * Temporarily unshare all files.  Some protocols (eg OpenFT) implement
	 * this with a single command disabling shares instead of removing them.
	 * To my knowledge, most other protocols will require you actually
	 * unshare every file manually.
	 */
	void (*share_hide) (struct protocol *p);

	/**
	 * Enable sharing after it was previously hidden.
	 */
	void (*share_show) (struct protocol *p);

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
	int (*stats) (struct protocol *p,
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
	int (*source_cmp) (struct protocol *p, struct source *a, struct source *b);

	/**
	 * Compare two users.  This was specially added for OpenFT so that node
	 * aliases could be weeded out as a significant portion of the username
	 * field.  Default implementation here is a simple strcmp.
	 */
	int (*user_cmp) (struct protocol *p, char *a, char *b);
} Protocol;

/*****************************************************************************/

/**
 * Attempts to determine if the communication defined here (libgiftproto) is
 * compatible with the current plugin or giftd binary.  All protocol plugins
 * are expected to call this first in Protocol::init and return FALSE from
 * there if the runtime versions are not compatible.
 *
 * Please note that you should specify the version that you know to be
 * compatible.  That is, the version that you were working from when
 * developing, or the maximum possible compatible version that you know of.
 * Please allow libgiftproto to determine compatibility beyond that point.
 *
 * @param version  Version constructed from ::LIBGIFTPROTO_MKVERSION.
 *
 * @return If the return value is less than, equal to, or greater than,
 *         the version supplied is too low, compatible, or too great for the
 *         current libgiftproto runtime.
 */
int protocol_compat (uint32_t version);

/**
 * Create a new protocol structure.  This completely initializes the function
 * pointer table with dummy functions expecting that the caller (giFT) fill
 * in the _actual_ implementations.  Also adds to the list of active
 * protocols.
 *
 * @param name     Protocol name to construct.
 * @param version  Calls ::protocol_compat with this argument as a
 *                 convenience to the giFT daemon.
 */
Protocol *protocol_new (char *name, uint32_t version);

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
 * Loop through all 'active' protocols.  See ::dataset_foreach.
 *
 * @param func
 * @param udata
 */
void protocol_foreach (DatasetForeachFn func, void *udata);

/**
 * Same as ::protocol_foreach except that ::dataset_foreach_ex will be used,
 * allowing you to remove protocols as you iterate.
 */
void protocol_foreachclear (DatasetForeachExFn func, void *udata);

/*****************************************************************************/

#ifdef __cplusplus
}
#endif /* __reallycrappylanguage */

/*****************************************************************************/

#endif /* __PROTOCOL_H */
