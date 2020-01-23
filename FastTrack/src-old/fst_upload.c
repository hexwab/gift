/*
 * $Id: fst_upload.c,v 1.6 2003/12/02 19:50:34 mkern Exp $
 *
 * Copyright (C) 2003 giFT-FastTrack project
 * http://developer.berlios.de/projects/gift-fasttrack
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

#include "fst_fasttrack.h"
#include "fst_upload.h"


#define LOG_HTTP_HEADERS


/*****************************************************************************/

/* sends http error reply */
static void upload_send_error_reply (TCPC *tcpcon, int code);

/* adds meta tag to http header */
static void upload_add_meta_header (ds_data_t *key, ds_data_t *value,
                                    FSTHttpHeader *reply);

/* sends http success reply */
static int upload_send_success_reply (FSTUpload *upload);

/* parses things like user name, range, etc. from request */
static int upload_parse_request (FSTUpload *upload);

/* opens file for reading */
static FILE *upload_open_share (Share *share);

static void upload_send_file (int fd, input_id input, FSTUpload *upload);

static void upload_write_gift (FSTUpload *upload, unsigned char *data,
                               unsigned int len);

static void upload_error_gift (FSTUpload *upload, unsigned short klass,
                               char *error);

/*****************************************************************************/

/* called by http server for every received GET request */
int fst_upload_process_request (FSTHttpServer *server, TCPC *tcpcon,
                                FSTHttpHeader *request)
{
	FSTUpload *upload;
	Share *share;
	unsigned char *hash;
	int hash_len, auth;

	/* if we don't share try no further */
	if (!FST_PLUGIN->allow_sharing || FST_PLUGIN->hide_shares)
	{
		FST_DBG_1 ("rejecting http request from \"%s\" because we are not sharing",
		           net_ip_str (tcpcon->host));
		upload_send_error_reply (tcpcon, 404);
		return FALSE;
	}

	/* extract hash from URI and look up share */
	if (strncmp (request->uri, "/.hash=", 7) != 0)
	{
		FST_DBG_2 ("Invalid uri \"%s\" from %s",
		           request->uri, net_ip_str (tcpcon->host));
		upload_send_error_reply (tcpcon, 400);
		return FALSE;
	}

	hash = fst_utils_hex_decode (request->uri + 7, &hash_len);

	if (!hash || hash_len != FST_HASH_LEN)
	{
		FST_DBG_2 ("Non-hash uri \"%s\" from %s",
		           request->uri, net_ip_str (tcpcon->host));
		upload_send_error_reply (tcpcon, 400);
		free (hash);
		return FALSE;
	}

	share = FST_PROTO->share_lookup (FST_PROTO, SHARE_LOOKUP_HASH,
	                                 FST_HASH_NAME, hash, FST_HASH_LEN);
	free (hash);

	if (!share)
	{
		FST_DBG_2 ("No file found for uri \"%s\" from %s",
		           request->uri, net_ip_str (tcpcon->host));
		upload_send_error_reply (tcpcon, 404);
		return FALSE;
	}

	/* we found a share, create upload object */
	if (! (upload = fst_upload_create (tcpcon, request)))
	{
		FST_ERR_2 ("fst_upload_create failed for uri \"%s\" from %s",
		            request->uri, net_ip_str (tcpcon->host));
		upload_send_error_reply (tcpcon, 500);
		return FALSE;
	}

	upload->share = share;

	/* parse necessary things out of header */
	if (!upload_parse_request (upload))
	{
		FST_ERR_2 ("upload_parse_request failed for uri \"%s\" from %s",
		           request->uri, net_ip_str (tcpcon->host));
		upload_send_error_reply (tcpcon, 400);
		fst_upload_free (upload);
		return TRUE; /* fst_upload_free closed connection and freed request */
	}
	
	/* check if there is an upload slot available */
	auth = FST_PROTO->upload_auth (FST_PROTO, upload->user, share, NULL);

	if (auth == UPLOAD_AUTH_NOTSHARED ||
		auth == UPLOAD_AUTH_HIDDEN ||
		auth == UPLOAD_AUTH_STALE)
	{
		FST_DBG_2 ("File \"%s\" requested by %s not shared/hidden/stale",
		           share->path, upload->user);
		upload_send_error_reply (tcpcon, 404);
		fst_upload_free (upload);
		return TRUE;
	}
	
	if (auth == UPLOAD_AUTH_MAX ||
		auth == UPLOAD_AUTH_MAX_PERUSER)
	{
		FST_DBG_1 ("No upload slot available for %s", upload->user);
		upload_send_error_reply (tcpcon, 503);
		fst_upload_free (upload);
		return TRUE;
	}

	if (auth != UPLOAD_AUTH_ALLOW)
	{
		FST_ERR_3 ("Unknown reply code from upload_auth: %d for file \"%s\" to %s",
		           auth, share->path, upload->user);
		upload_send_error_reply (tcpcon, 404);
		fst_upload_free (upload);
		return TRUE;
	}

	/* open file for reading */
	if (! (upload->file = upload_open_share (upload->share)))
	{
		FST_DBG_2 ("Unable to open file \"%s\" for %s",
		           share->path, upload->user);
		upload_send_error_reply (tcpcon, 404);
		fst_upload_free (upload);
		return TRUE;		
	}

	/* seek to start position */
	if (fseek (upload->file, upload->start, SEEK_SET) != 0)
	{
		FST_DBG_3 ("seek to %d failed for file \"%s\" to %s",
		           upload->start, share->path, upload->user);
		upload_send_error_reply (tcpcon, 404);
		fst_upload_free (upload);
		return TRUE;		
	}

	/* create upload */
	upload->transfer = FST_PROTO->upload_start (FST_PROTO, &upload->chunk,
	                                            upload->user, upload->share,
	                                            upload->start, upload->stop);

	if (!upload->transfer)
	{
		FST_ERR_2 ("upload_start failed for file \"%s\" to %s",
		           share->path, upload->user);
		upload_send_error_reply (tcpcon, 500);
		fst_upload_free (upload);
		return TRUE;		
	}

	/* send success reply */
	if (!upload_send_success_reply (upload))
	{
		FST_ERR_2 ("upload_send_success_reply failed for \"%s\" to %s",
		           share->path, upload->user);
		fst_upload_free (upload);
		return TRUE;		
	}

	upload->chunk->udata = upload;

	FST_DBG_2 ("started upload of \"%s\" to %s", share->path, upload->user);

	/* send file */
	input_add (upload->tcpcon->fd, (void*)upload, INPUT_WRITE,
			   (InputCallback)upload_send_file, 0);

	return TRUE;
}

/*****************************************************************************/

/* called by gift to stop upload on user's request */
void fst_giftcb_upload_stop (Protocol *p, Transfer *transfer,
                             Chunk *chunk, Source *source)
{
	FSTUpload *upload = (FSTUpload*) chunk->udata;

	if (!upload)
	{
		FST_DBG_1 ("chunk->udata == NULL for upload to %s, doing nothing",
		           upload->user);
		return;
	}

	FST_DBG_2 ("finished upload to %s, transferred %d bytes",
	           upload->user, chunk->transmit);

	fst_upload_free (upload);
}

/*****************************************************************************/

/* alloc and init upload */
FSTUpload *fst_upload_create (TCPC *tcpcon, FSTHttpHeader *request)
{
	FSTUpload *upload;

	if (! (upload = malloc (sizeof (FSTUpload))))
		return NULL;

	if (! (upload->data = malloc (FST_UPLOAD_BUFFER_SIZE)))
	{
		free (upload);
		return NULL;
	}

	upload->transfer = NULL;
	upload->chunk = NULL;	
	upload->share = NULL;

	upload->request = request;
	upload->user = NULL;
	upload->start = 0;
	upload->stop = 0;

	upload->tcpcon = tcpcon;
	upload->file = NULL;

	return upload;
}

/* free push */
void fst_upload_free (FSTUpload *upload)
{
	if (!upload)
		return;

	if (upload->file)
		fclose (upload->file);

	tcp_flush (upload->tcpcon, TRUE);
	tcp_close (upload->tcpcon);
	fst_http_header_free (upload->request);

	free (upload->user);
	free (upload->data);

	free (upload);
}

/*****************************************************************************/

/* sends http error reply */
static void upload_send_error_reply (TCPC *tcpcon, int code)
{
	FSTHttpHeader *reply;
	String *reply_str;
	char *value;

	if ((reply = fst_http_header_reply (HTHD_VER_11, code)))
	{
		fst_http_header_set_field (reply, "Server", FST_HTTP_SERVER);

		if (code == 503)
		{
			/* TODO: makes this dependant on length of upload queue */
			fst_http_header_set_field (reply, "Retry-After", "300");
		}

		fst_http_header_set_field (reply, "X-Kazaa-Username", FST_USER_NAME);
		fst_http_header_set_field (reply, "X-Kazaa-Network", FST_NETWORK_NAME);
		
		if (FST_PLUGIN->server)
		{
			value = stringf ("%s:%d",
			                 net_ip_str (FST_PLUGIN->external_ip),
					         FST_PLUGIN->server->port);
			fst_http_header_set_field (reply, "X-Kazaa-IP", value);
		}

		if (FST_PLUGIN->session && FST_PLUGIN->session->state == SessEstablished)
		{
			value = stringf ("%s:%d",
			                 net_ip_str (FST_PLUGIN->session->tcpcon->host),
					         FST_PLUGIN->session->tcpcon->port);
			fst_http_header_set_field (reply, "X-Kazaa-SupernodeIP", value);
		}
			
		if ((reply_str = fst_http_header_compile (reply)))
		{
#ifdef LOG_HTTP_HEADERS
	FST_HEAVY_DBG_3 ("sending http reply to %s:%d:\r\n%s",
	                 net_ip_str(tcpcon->host), tcpcon->port, reply_str->str);
#endif

			if (tcp_writestr (tcpcon, reply_str->str) < 0)
			{
				FST_DBG_2 ("ERROR: tcp_writestr failed for %s:%d",
				           net_ip_str(tcpcon->host), tcpcon->port);
			}
			
			tcp_flush (tcpcon, TRUE);
			string_free (reply_str);
		}

		fst_http_header_free (reply);
	}
}


/* adds meta tag to http header */
static void upload_add_meta_header (ds_data_t *key, ds_data_t *value,
                                    FSTHttpHeader *reply)
{
	FSTFileTag tag;
	char *http_str;

	if ((tag = fst_meta_tag_from_name (key->data)) == FILE_TAG_ANY)
		return;

	if (! (http_str = fst_meta_httpstr_from_giftstr (key->data, value->data)))
		return;

	fst_http_header_set_field (reply, "X-KazaaTag", stringf ("%u=%s", tag, http_str));

	free (http_str);
}

/* sends http success reply */
static int upload_send_success_reply (FSTUpload *upload)
{
	FSTHttpHeader *reply;
	String *reply_str;
	char *value;
	Hash *hash;

	if (! (reply = fst_http_header_reply (HTHD_VER_11, 206)))
		return FALSE;

	fst_http_header_set_field (reply, "Server", FST_HTTP_SERVER);
	fst_http_header_set_field (reply, "Connection", "close");
	fst_http_header_set_field (reply, "Accept-Ranges", "bytes");
	fst_http_header_set_field (reply, "X-Kazaa-Username", FST_USER_NAME);
	fst_http_header_set_field (reply, "X-Kazaa-Network", FST_NETWORK_NAME);
		
	if (FST_PLUGIN->server)
	{
		value = stringf ("%s:%d",
		                 net_ip_str (FST_PLUGIN->external_ip),
				         FST_PLUGIN->server->port);
		fst_http_header_set_field (reply, "X-Kazaa-IP", value);
	}

	if (FST_PLUGIN->session && FST_PLUGIN->session->state == SessEstablished)
	{
		value = stringf ("%s:%d",
		                 net_ip_str (FST_PLUGIN->session->tcpcon->host),
				         FST_PLUGIN->session->tcpcon->port);
		fst_http_header_set_field (reply, "X-Kazaa-SupernodeIP", value);
	}
			
	fst_http_header_set_field (reply, "Content-Type", upload->share->mime);

	/* construct the Content-Range reply */
	value = stringf ("bytes %lu-%lu/%lu",
	                 (unsigned long)(upload->start),
	                 (unsigned long)(upload->stop - 1),
					 (unsigned long)(upload->share->size));
	fst_http_header_set_field (reply, "Content-Range", value);

	/* total content length */
	value = stringf ("%lu", (unsigned long)(upload->stop - upload->start));
	fst_http_header_set_field (reply, "Content-Length", value);
	
#if 0
	/* NOTE: FastTrack uses multiple X-KazaaTag http headers which currently
	 * is not supported by our http header implementation.
	 * We thus only send the X-KazaaTag for the hash.
	 * The http spec states that we could just put everything in one field
	 * and comma separate it but i doubt Kazaa would understand that.
	 */

	/* add X-KazaaTag meta tags */
	share_foreach_meta (upload->share,
	                    (DatasetForeachFn) upload_add_meta_header,
	                    (void*)reply);
#endif

	/* add X-KazaaTag tag for hash */
	if ((hash = share_get_hash (upload->share, FST_HASH_NAME)))
	{
		assert (hash->len == FST_HASH_LEN);

		value = fst_utils_base64_encode (hash->data, hash->len);

		fst_http_header_set_field (reply, "X-KazaaTag",
								   stringf ("%u==%s", FILE_TAG_HASH, value));
		free (value);
	}

	/* compile and send header */
	if (! (reply_str = fst_http_header_compile (reply)))
	{
		fst_http_header_free (reply);
		return FALSE;
	}

#ifdef LOG_HTTP_HEADERS
	FST_HEAVY_DBG_3 ("sending http reply to %s:%d:\r\n%s",
	                 net_ip_str(upload->tcpcon->host), upload->tcpcon->port,
	                 reply_str->str);
#endif

	if (tcp_writestr (upload->tcpcon, reply_str->str) < 0)
	{
		FST_DBG_2 ("ERROR: tcp_writestr failed for %s:%d",
		           net_ip_str(upload->tcpcon->host), upload->tcpcon->port);
		string_free (reply_str);
		fst_http_header_free (reply);
		return FALSE;
	}
			
	tcp_flush (upload->tcpcon, TRUE);

	string_free (reply_str);
	fst_http_header_free (reply);

	return TRUE;
}

/*****************************************************************************/

/* parses things like user name, range, etc. from request */
static int upload_parse_request (FSTUpload *upload)
{
	char *buf, *buf0;
	
	if (!upload)
		return FALSE;

	/* create user name */
	if((buf = fst_http_header_get_field (upload->request, "X-Kazaa-Username")))
	{
		upload->user = stringf_dup ("%s@%s", buf,
		                            net_ip_str (net_peer (upload->tcpcon->fd)));
	}
	else
	{
		upload->user = strdup (net_ip_str (net_peer (upload->tcpcon->fd)));
	}

	/* get range */
	buf = buf0 = gift_strdup (fst_http_header_get_field (upload->request, "Range"));

	if (buf)
	{
		if (!string_sep (&buf, "bytes=") || !buf)
		{
			free (buf0);
			return FALSE;
		}

		upload->start = (off_t)(gift_strtoul (string_sep (&buf, "-")));
		upload->stop  = (off_t)(gift_strtoul (string_sep (&buf, " ")));

		free (buf0);

		if (upload->stop == 0)
			return FALSE;

		upload->stop += 1; /* http range is inclusive, giFT's not */

		if (upload->stop > upload->share->size)
			return FALSE;
	}
	else
	{
		/* no range was given, send entire file */
		upload->start = 0;
		upload->stop = upload->share->size;
	}

	return TRUE;
}

/* opens file for reading */
static FILE *upload_open_share (Share *share)
{
	FILE *f;
	char *host_path;

	if (!(host_path = file_host_path (share->path)))
		return NULL;

	f = fopen (host_path, "rb");
	free (host_path);

	return f;
}

/*****************************************************************************/

static void upload_send_file (int fd, input_id input, FSTUpload *upload)
{
	int read_len, send_len, sent_len;

	if (net_sock_error (fd))
	{
		/* connection closed */
		FST_HEAVY_DBG_1 ("net_sock_error(fd) for %s",
		                 upload->user);
		input_remove (input);
		upload_error_gift (upload, SOURCE_CANCELLED, "Remote cancelled");
		return;
	}

	/* ask giFT how much we should send */
	if ((send_len = upload_throttle (upload->chunk, FST_UPLOAD_BUFFER_SIZE)) == 0)
		return;

	/* read from the file the number of bytes we plan to send */
	if ((read_len = fread (upload->data, 1, send_len, upload->file)) == 0)
	{
		FST_ERR_1 ("unable to read upload share for %s",
		           upload->user);
		input_remove (input);
		upload_error_gift (upload, SOURCE_CANCELLED, "Local read error");
		return;
	}

	/* write the block */
	if ((sent_len = tcp_send (upload->tcpcon, upload->data, read_len)) <= 0)
	{
		FST_HEAVY_DBG_1 ("unable to send data for %s",
		                 upload->user);
		input_remove (input);
		upload_error_gift (upload, SOURCE_CANCELLED, "Send error");
		return;
	}

	/* short write, rewind our fread to match */
	if (sent_len < read_len)
	{
		FST_DBG_1 ("short write, rewinding read stream for %s",
		           upload->user);

		if ((fseek (upload->file, -((off_t)(read_len - sent_len)), SEEK_CUR)) != 0)
		{
			FST_ERR_1 ("unable to seek back for %s", upload->user);
			input_remove (input);
			upload_error_gift (upload, SOURCE_CANCELLED, "Local seek error");
			return;
		}
	}

	/* this may remove us via fst_giftcb_upload_stop if transfer is complete */
	upload_write_gift (upload, upload->data, sent_len);

	/* wait for next call */
}	

/*****************************************************************************/

static void upload_write_gift (FSTUpload *upload, unsigned char *data,
                               unsigned int len)
{
	FST_PROTO->chunk_write (FST_PROTO, upload->chunk->transfer, upload->chunk,
							upload->chunk->source, data, len);
}

static void upload_error_gift (FSTUpload *upload, unsigned short klass,
                               char *error)
{
	FST_PROTO->source_status (FST_PROTO, upload->chunk->source, klass, error);
	/* tell giFT an error occured with this download */
	upload_write_gift (upload, NULL, 0);
}

/*****************************************************************************/
