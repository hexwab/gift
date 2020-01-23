/*
 * $Id: ft_http.h,v 1.4 2003/11/02 12:09:04 jasta Exp $
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

#ifndef __FT_HTTP_H
#define __FT_HTTP_H

/*****************************************************************************/

/**
 * @file ft_http.h
 *
 * @brief Helper tools for managing HTTP request and reply objects.
 *
 * This system is used by both client and server as both must be able to
 * process requests and replies.  Please note that the efficiency here leaves
 * a lot to be desired, but I just wanted to cut through this code as quickly
 * as possible.
 *
 * This system may eventually unify both objects into a single base class
 * interface, but for now I'm not sure it warrants such complexity.
 */

/*****************************************************************************/

/**
 * HTTP reply object capable of parsing and managing a single HTTP header
 * reply.
 */
typedef struct
{
	float    version;                  /**< HTTP version (1.0, 1.1, ...) */
	int      code;                     /**< HTTP response code */
	Dataset *keylist;                  /**< Set of parsed key: value entries */
} FTHttpReply;

/**
 * HTTP request object used to parse and construct HTTP request headers.
 */
typedef struct
{
	char    *method;                   /**< HTTP method (eg GET, POST, ...) */
	char    *request;                  /**< HTTP request (eg /foo/bar) */
	Dataset *keylist;                  /**< Set of keys to deliver with the
	                                    *   request */
} FTHttpRequest;

/*****************************************************************************/

/**
 * Simple interface for translating HTTP codes into the string meaning
 * according to the HTTP RFC.  For example, an input parameter of 206
 * would result in "Partial Content".
 *
 * @param code  HTTP code.  Note that an extremely limited set of codes are
 *              defined here.
 *
 * @return Pointer to internal memory referencing the NUL-terminated
 *         string definition.
 */
char *http_code_string (int code);

/**
 * Simple helper function used to determine completeness of an HTTP request
 * or reply.
 *
 * @return TRUE If complete.  That is, there exists two subsequent \n
 *         characters, optionally preceeded by \r.
 */
BOOL http_check_sentinel (char *data, size_t len);

/*****************************************************************************/

/*
 * Note to the reader: these functions do not operate on complete URLs,
 * instead they operate on request body portions of the scheme-specific
 * part of the URL.  That is, "/path/to/foo", but not
 * "http://foo.com/path/to/foo".
 */
char *http_url_encode (const char *unencoded);
char *http_url_decode (const char *encoded);

/*****************************************************************************/

/**
 * Construct a new reply object.  Consider using ::ft_http_reply_unserialize
 * if that is what you really want to do.
 *
 * @param code  HTTP reply code to use by default.  You may change this at
 *              any time by directly accessing the code member of the object.
 */
FTHttpReply *ft_http_reply_new (int code);

/**
 * Destroy a constructed or unserialized reply object.  This will handle
 * cleanup of all internally managed memory, including anything duplicated
 * off the original serialized data stream.
 */
void ft_http_reply_free (FTHttpReply *reply);

/**
 * Unserialize a reply sent from a remote user.  This function is called
 * after we know we have found two consecutive blocks of CRLF or LF so
 * the data will look something like:
 *
 * HTTP/1.0 200 OK [CRLF]
 * Content-Length: 1024 [CRLF]
 * Content-Type: text/plain [CRLF]
 * X-Foo: Bar [CRLF]
 * [CRLF]
 *
 * @param data  Writable data segment containing the text to be parsed.
 *
 * @return Newly constructed reply object, or NULL on error.
 */
FTHttpReply *ft_http_reply_unserialize (char *data);

/**
 * Serialize a reply object for writing over the socket.  Consider using the
 * helper function ::ft_http_reply_send, which calls this internally.
 *
 * @return Dynamically allocated serialized data segment.
 */
char *ft_http_reply_serialize (FTHttpReply *reply, size_t *retlen);

/**
 * Serializes and then writes to the connection named using ::tcp_write
 * (buffered).  This method also takes over memory management of the reply
 * object.
 *
 * @return Direct from ::tcp_write, or -1 when ::ft_http_reply_serialize
 *         fails.
 */
int ft_http_reply_send (FTHttpReply *reply, TCPC *c);

/*****************************************************************************/

/**
 * Create a new request object, similar to ::ft_http_reply_new.
 *
 * @param method   HTTP request method (eg GET or HEAD).
 * @param request  Request URI.
 */
FTHttpRequest *ft_http_request_new (const char *method, const char *request);

/**
 * Destroy a request object.
 */
void ft_http_request_free (FTHttpRequest *req);

/**
 * Parse and construct a request object from an serialized HTTP reply.  This
 * is used to parse incoming requests and work with the object similarly to
 * if we were constructing a reply for delivery.
 */
FTHttpRequest *ft_http_request_unserialize (char *data);

/**
 * Serialize an HTTP reply.  See ::ft_http_request_unserialize.
 *
 * @param req
 * @param retlen  Stores the length of the serialized message, if non-NULL.
 *
 * @return Dynamically allocated NUL-terminated serialized message.
 */
char *ft_http_request_serialize (FTHttpRequest *req, size_t *retlen);

/**
 * Serialize and send a constructed request object.
 *
 * @return Direct from ::tcp_write.
 */
int ft_http_request_send (FTHttpRequest *req, TCPC *c);

/*****************************************************************************/

#endif /* __FT_HTTP_H */
