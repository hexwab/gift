/*
 * $Id: fst_download.c,v 1.29 2004/12/28 16:37:27 mkern Exp $
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
#include "fst_download.h"

/*****************************************************************************/

static int download_client_callback (FSTHttpClient *client,
									 FSTHttpClientCbCode code);
static void download_write_gift (Source *source, unsigned char *data,
								 unsigned int len);
static void download_error_gift (Source *source, int remove_source,
								 unsigned short klass, char *error);
static char *download_calc_xferuid (char *uri);

/* returns supernode session by supernode ip */
static FSTSession *session_from_ip (in_addr_t ip);

/*****************************************************************************/

/* We need cannot directly abort sources from fst_giftcb_download_start so we
 * use this timer.
 */
static BOOL abort_source_func (Source *source)
{
	assert (source);
	assert (source->udata == NULL);

	/* this will call fst_giftcb_source_remove */
	FST_PROTO->source_abort (FST_PROTO, source->chunk->transfer, source);

	return FALSE; /* Remove timer. */
}

static void async_abort_source (Source *source)
{
	if (source->udata)
	{
		/* If there is still a http client around remove it now. */
		FST_HEAVY_DBG_1 ("scheduling removal of source %s", source->url);
		fst_http_client_free (source->udata);
		source->udata = NULL;
	}

	/* Abort source immediately after we returned to event loop. */
	timer_add (0, (TimerCallback)abort_source_func, source);
}


/* called by gift to start downloading of a chunk */
int fst_giftcb_download_start (Protocol *p, Transfer *transfer, Chunk *chunk,
							   Source *source)
{
	FSTSource *src;
	FSTPush *push;
	FSTSession *session;

	if (!(src = fst_source_create_url (source->url)))
	{
		/* this url is broken, remove the source */
		FST_WARN_1 ("malformed url \"%s\", removing source", source->url);
		async_abort_source (source);
		return FALSE;
	}

	/* determine whether we need to send a push. */
	if (fst_source_firewalled (src))
	{
		/* check if we already sent a push for this source */
		if ((push = fst_pushlist_lookup_source (FST_PLUGIN->pushlist, source)))
		{
			/* We already sent a push request for this source, remove it.
			 * Actually we shouldn't even get here since we removed the push in
			 * fst_giftcb_download_stop
			 */
			FST_WARN_2 ("removing old push for %s with id %d",
						source->url, push->push_id);
			fst_pushlist_remove (FST_PLUGIN->pushlist, push);
			fst_push_free (push);
			fst_source_free (src);
		}

		/* Find correct supernode to send push to. */
		if (!(session = session_from_ip (src->parent_ip)))
		{
			fst_source_free (src);

			/* We are no longer connected to the supernode this push should go
			 * to. Remove the source.
			 */
			FST_HEAVY_DBG_1 ("No supernode for sending push, removing source %s",
			                 source->url);

			async_abort_source (source);
			return FALSE;
		}

		fst_source_free (src);

		/* create push and add to list */
		if (! (push = fst_pushlist_add (FST_PLUGIN->pushlist, source)))
			return FALSE;

		/* send push request */
		if (!fst_push_send_request (push, session))
		{
			FST_DBG_1 ("push send failed, removing source %s", source->url);
			fst_pushlist_remove (FST_PLUGIN->pushlist, push);
			fst_push_free (push);

			async_abort_source (source);
			return FALSE;
		}

		FST_PROTO->source_status (FST_PROTO, source, SOURCE_WAITING, "Sent push");
		return TRUE;
	}

	fst_source_free (src);

	if (!fst_download_start (source, NULL))
	{
		FST_DBG ("fst_download_start failed");
		return FALSE;
	}

	return TRUE;
}

/* called by gift to stop download */
void fst_giftcb_download_stop (Protocol *p, Transfer *transfer, Chunk *chunk,
							   Source *source, int complete)
{
	FSTHttpClient *client = (FSTHttpClient*) source->udata;
	FSTPush *push;

	/* close connection if there is outstanding data */
	if (client && client->state != HTCL_CONNECTED)
	{
		fst_http_client_cancel (client);
		FST_HEAVY_DBG_1 ("request cancelled for url %s", source->url);
	}

	/* remove push */
	if ((push = fst_pushlist_lookup_source (FST_PLUGIN->pushlist, source)))
	{
		FST_HEAVY_DBG_2 ("removing push for %s with id %d",
						 source->url, push->push_id);
		fst_pushlist_remove (FST_PLUGIN->pushlist, push);
		fst_push_free (push);
	}
}

/* called by gift to add a source */
BOOL fst_giftcb_source_add (Protocol *p,Transfer *transfer, Source *source)
{
	FSTSource *src;

	/* prepare source */
	assert (source->udata == NULL);
	source->udata = NULL;

	/* parse url */
	if (!(src = fst_source_create_url (source->url)))
	{
		/* this url is broken, reject it */
		FST_WARN_1 ("malformed url, rejecting source \"%s\"", source->url);
		return FALSE;
	}

	/* firewalled sources may be rejected for various reasons */
	if (fst_source_firewalled (src))
	{
		/* we must have data for pushing */
		if (!fst_source_has_push_info (src))
		{
			FST_WARN_1 ("no push data, rejecting fw source \"%s\"", source->url);
			fst_source_free (src);
			return FALSE;	
		}

		/* we must be able to receive connections */
		if (!FST_PLUGIN->server)
		{
			FST_DBG_1 ("no server listening, rejecting fw source \"%s\"",
			           source->url);
			fst_source_free (src);
			return FALSE;
		}

		/* we must not be firewalled ourselves */
		if (FST_PLUGIN->external_ip != FST_PLUGIN->local_ip &&
			!FST_PLUGIN->forwarding)
		{
			FST_DBG_1 ("NAT detected but port is not forwarded, rejecting source %s",
			           source->url);
			fst_source_free (src);
			return FALSE;
		}

		/* we must still be connected to the correct supernode */
		if (!session_from_ip (src->parent_ip))
		{
			FST_DBG_1 ("no longer connected to correct supernode, rejecting source %s",
			           source->url);
			fst_source_free (src);
			return FALSE;
		}
	}

	fst_source_free (src);

	return TRUE;
}

/* called by gift to remove source */
void fst_giftcb_source_remove (Protocol *p, Transfer *transfer,
							   Source *source)
{
	FSTHttpClient *client = (FSTHttpClient*) source->udata;
	FSTPush *push;

	if (client)
	{
		FST_HEAVY_DBG_1 ("removing source %s", source->url);
		fst_http_client_free (client);
		source->udata = NULL;
	}

	/* remove push */
	if ((push = fst_pushlist_lookup_source (FST_PLUGIN->pushlist, source)))
	{
		FST_HEAVY_DBG_2 ("removing push for %s with id %d",
						 source->url, push->push_id);
		fst_pushlist_remove (FST_PLUGIN->pushlist, push);
		fst_push_free (push);
	}
}

/*****************************************************************************/

/* start download for source, optionally using existing tcpcon */
int fst_download_start (Source *source, TCPC *tcpcon)
{
	Chunk *chunk = source->chunk;
	FSTHttpClient *client = (FSTHttpClient*) source->udata;
	FSTHttpHeader *request;
	char *uri, *range_str;
	FSTSource *src;
	FSTHash *hash;
	char *hash_algo;

	assert (source);
	assert (chunk);

	/* parse hash */
	if (!(hash = fst_hash_create ()))
		return FALSE;

	if (!(hash_algo = hashstr_algo (source->hash)))
	{
		FST_WARN_2 ("invalid hash %s supplied with source \"%s\"",
		            source->hash, source->url);
		fst_hash_free (hash);
		return FALSE;
	}

	if (!gift_strcasecmp (hash_algo, FST_KZHASH_NAME))
	{
		if (!fst_hash_decode16_kzhash (hash, hashstr_data (source->hash)))
		{
			FST_WARN_2 ("invalid hash %s supplied with source \"%s\"",
			            source->hash, source->url);
			fst_hash_free (hash);	
			return FALSE;
		}
	}
	else if (!gift_strcasecmp (hash_algo, FST_FTHASH_NAME))
	{
		if (!fst_hash_decode64_fthash (hash, hashstr_data (source->hash)))
		{
			FST_WARN_2 ("invalid hash %s supplied with source \"%s\"",
			            source->hash, source->url);
			fst_hash_free (hash);	
			return FALSE;
		}
	}
	else
	{
		FST_WARN_2 ("invalid hash %s supplied with source \"%s\"",
		            source->hash, source->url);
		fst_hash_free (hash);	
		return FALSE;
	}

	/* create uri from hash */
	uri = stringf_dup ("/.hash=%s", fst_hash_encode16_fthash (hash));
	fst_hash_free (hash);

	/* parse url */
	if (!(src = fst_source_create_url (source->url)))
	{
		FST_WARN_1 ("malformed url %s", source->url);
		free (uri);
		return FALSE;
	}

	/* create http request */
	if (! (request = fst_http_header_request (HTHD_VER_11, HTHD_GET, uri)))
	{
		FST_WARN_1 ("creation of request failed for url %s", source->url);
		free (uri);
		fst_source_free (src);
		return FALSE;
	}

	/* remove old client and create new one if pushed connection */
	if (tcpcon)
	{
		FST_HEAVY_DBG_1 ("creating client for pushed connection from source %s",
						 source->url);

		fst_http_client_free (client);
		client = fst_http_client_create_tcpc (tcpcon, download_client_callback);

		client->udata = (void*) source;
		source->udata = (void*) client;
	}
	else if (!client)
	{
		FST_HEAVY_DBG_1 ("first time use of source %s", source->url);

		client = fst_http_client_create (net_ip_str (src->ip), src->port,
										 download_client_callback);
		client->udata = (void*) source;
		source->udata = (void*) client;
	}

	/* add some http headers */
	fst_http_header_set_field (request, "UserAgent", FST_HTTP_AGENT);
	fst_http_header_set_field (request, "X-Kazaa-Network", FST_NETWORK_NAME);
	fst_http_header_set_field (request, "X-Kazaa-Username", FST_USER_NAME);

	/* http range is inclusive!
	 * use chunk->start + chunk->transmit for starting point, rather non-intuitive
	 */
	range_str = stringf ("bytes=%d-%d", (int)(chunk->start + chunk->transmit),
						 (int)chunk->stop - 1);
	fst_http_header_set_field (request, "Range", range_str);

#ifdef FST_DOWNLOAD_BOOST_PL
	fst_http_header_set_field (request, "X-Kazaa-XferUid",
							   download_calc_xferuid (uri));
#endif

	free (uri);
	fst_source_free (src);

	/* make the request */
	FST_PROTO->source_status (FST_PROTO, source, SOURCE_WAITING, "Connecting");

	if (!fst_http_client_request (client, request, FALSE))
	{
		FST_WARN_1 ("request failed for url %s", source->url);
		return FALSE;
	}

	return TRUE;
}

/*****************************************************************************/

/* http client callback */
static int download_client_callback (FSTHttpClient *client,
									 FSTHttpClientCbCode code)
{
	Source *source = (Source*) client->udata;
	assert (source);

	switch (code)
	{
	case HTCL_CB_CONNECT_FAILED:
		download_error_gift (source, TRUE, SOURCE_TIMEOUT, "Connect failed");
		break;

	case HTCL_CB_REQUESTING:
		FST_PROTO->source_status (FST_PROTO, source, SOURCE_WAITING,
								  "Requesting");
		break;

	case HTCL_CB_REQUEST_FAILED:
/*
		download_error_gift (source, TRUE, SOURCE_TIMEOUT, "Request failed");
*/
		download_error_gift (source, FALSE, SOURCE_TIMEOUT, "Request failed");
		break;
	
	case HTCL_CB_REPLIED:
	{
		FSTHttpHeader *reply = client->reply;
		char *p;

		/* check reply code */
		if (reply->code != 200 && reply->code != 206)
		{
			FST_HEAVY_DBG_3 ("got reply code %d (\"%s\") for url %s -> aborting d/l",
							 reply->code, reply->code_str, source->url);

			switch (reply->code)
			{
			case 503:
				download_error_gift (source, FALSE, SOURCE_QUEUED_REMOTE,
									 "Remotely queued");
				break;
			case 404:
				download_error_gift (source, TRUE, SOURCE_CANCELLED,
									 "File not found");
				break;
			default:
				download_error_gift (source, TRUE, SOURCE_CANCELLED,
									 "Weird http code");
			}

			return FALSE;
		}

		FST_HEAVY_DBG_3 ("got reply code %d (\"%s\") for url %s -> starting d/l",
						 reply->code, reply->code_str, source->url);

		/* make sure the start offset is correct */
		if ( (p = fst_http_header_get_field (reply, "Content-Range")))
		{
			int start, stop;
			sscanf (p, "bytes %d-%d", &start, &stop);

			/* check start offset, shorter/longer ranges are handled by giFT */
			if (start != source->chunk->start + source->chunk->transmit)
			{
				FST_WARN ("Removing source due to range mismatch");
				FST_WARN_2 ("\trequested range: %d-%d",
							source->chunk->start + source->chunk->transmit,
							source->chunk->stop - 1);
				FST_WARN_2 ("\treceived range: %d-%d", start, stop);
				FST_WARN_1 ("\tContent-Length: %s",
							fst_http_header_get_field (reply, "Content-Length"));

				download_error_gift (source, TRUE, SOURCE_CANCELLED,
									 "Range mismatch");
				return FALSE;
			}
		}

		FST_PROTO->source_status (FST_PROTO, source, SOURCE_ACTIVE, "Active");
		break;
	}

	case HTCL_CB_DATA:
		/* write data to file through giFT.
		 * this calls fst_giftcb_download_stop() when download is complete
		 *
		 * TODO: somehow use download_throttle here
		 */
		download_write_gift (source, client->data, client->data_len);
	
		break;

	case HTCL_CB_DATA_LAST:
		FST_HEAVY_DBG_3 ("HTCL_CB_DATA_LAST (%d/%d) for %s",
						 client->content_received, client->content_length,
						 source->url);

		if (client->data_len)
		{
			assert (client->content_length == client->content_received);
			/* last write
			 * this calls fst_giftcb_download_stop() when download is complete
			 */
			download_write_gift (source, client->data, client->data_len);
			break;
		}

		/* premature end of data */
		/* this makes giFT call fst_giftcb_download_stop() */
		download_error_gift (source, FALSE, SOURCE_CANCELLED,
							 "Cancelled remotely");
		return FALSE;
	}

	return TRUE; /* continue with request */
}

/*****************************************************************************/

static void download_write_gift (Source *source, unsigned char *data,
								 unsigned int len)
{
	FST_PROTO->chunk_write (FST_PROTO, source->chunk->transfer, source->chunk,
							source, data, len);
}

static void download_error_gift (Source *source, int remove_source,
								 unsigned short klass, char *error)
{
	if (remove_source)
	{
		FST_DBG_2 ("download error (\"%s\"), removing source %s",
				   error, source->url);
		FST_PROTO->source_status (FST_PROTO, source, klass, error);
		FST_PROTO->source_abort (FST_PROTO, source->chunk->transfer, source);
	}
	else
	{
		FST_PROTO->source_status (FST_PROTO, source, klass, error);
		/* tell giFT an error occured with this download */
		download_write_gift (source, NULL, 0);
	}
}

/*****************************************************************************/

#define SWAPU32(x) ((fst_uint32) ((((( \
((fst_uint8*)&(x))[0] << 8) | \
((fst_uint8*)&(x))[1]) << 8) | \
((fst_uint8*)&(x))[2]) << 8) | \
((fst_uint8*)&(x))[3])

/* returns static base64 encoded string for X-Kazaa-XferUid http header.
 * needs more work, need to figure out how last_search_hash is created.
 */
static char *download_calc_xferuid (char *uri)
{
	/* This search hash was provided by the guy who also insisted that the
	 * below time(NULL) MUST be time(0) to work correctly.
	 * The PL changes based on system time of uploading host.
	 * I presume current time is used in creation of last_search_hash.
	 * This hash was created in the distant future and seems to give
	 * a PL of at least 1000 even though it fluctuates.
	 */
	static const unsigned char last_search_hash[32] = {
		0x6f, 0xad, 0x17, 0x55, 0x60, 0x93, 0x31, 0x0e,
		0x05, 0x69, 0x0e, 0x1f, 0xee, 0x79, 0x39, 0x60,
		0xd7, 0x47, 0xa0, 0x34, 0x20, 0x94, 0x2b, 0xf8,
		0xd7, 0xc4, 0xd8, 0xe5, 0xba, 0xf3, 0xe2, 0x97
	};

	static const unsigned int VolumeId = 0xE09C4791;


	FSTCipher *cipher;
	unsigned int buf[8]; 
	static char base64[45], *base64_ptr;
	unsigned int uri_smhash, smhash;
	unsigned int seed;

	if (uri == NULL)
		return NULL;

	if (*uri == '/')
		uri++;

	uri_smhash = fst_hash_small (0xFFFFFFFF, uri, strlen(uri));

	memcpy (buf, last_search_hash, 32);
	seed = SWAPU32 (buf[0]);

	/* run through cipher */
	cipher = fst_cipher_create ();

	if (!fst_cipher_init (cipher, seed, 0xB0))
	{
		fst_cipher_free (cipher);
		base64[0] = 0;
		return base64;
	}

	fst_cipher_crypt (cipher, (unsigned char*)(buf+1), 28); 
	fst_cipher_free (cipher);

	seed = SWAPU32 (buf[1]);

	buf[1] = 0;
	smhash = fst_hash_small (0xFFFFFFFF, (unsigned char*)(buf+1), 28);

	/* weird */
	if( (seed != smhash) ||
		(SWAPU32(buf[2]) != VolumeId) ||
		(SWAPU32(buf[6]) >= 0x3B9ACA00) ||
		(SWAPU32(buf[7]) >= 0x3B9ACA00) ||
		(SWAPU32(buf[4]) >= 0x3B9ACA00) ||
		(SWAPU32(buf[5]) >= 0x3B9ACA00))
	{
		memset(buf, 0, 32);
	}

	seed = SWAPU32 (buf[3]) - time(NULL);	/* hmm */

	buf[3] = SWAPU32 (seed);
	buf[2] = SWAPU32 (uri_smhash);

	buf[1] = 0;
	smhash = fst_hash_small (0xFFFFFFFF, (unsigned char*)(buf+1), 28);
	buf[1] = SWAPU32 (smhash);

	seed = SWAPU32 (buf[3]);
	seed ^= smhash;

	buf[0] = SWAPU32 (seed);

	/* run through cipher */
	cipher = fst_cipher_create();

	if (!fst_cipher_init (cipher, seed, 0xB0))
	{
		fst_cipher_free (cipher);
		base64[0] = 0;
		return base64;
	}

	fst_cipher_crypt (cipher, (unsigned char*)(buf+1), 28); 
	fst_cipher_free (cipher);

	/* base64 encode */
	base64_ptr = fst_utils_base64_encode ( (unsigned char*)buf, 32);
	strncpy (base64, base64_ptr, 45);
	base64[44] = '\0';
	free (base64_ptr);
	
	return base64;
}

/*****************************************************************************/

/* returns supernode session by supernode ip */
static FSTSession *session_from_ip (in_addr_t ip)
{
	List *l;

	if (FST_PLUGIN->session && FST_PLUGIN->session->tcpcon->host == ip)
		return FST_PLUGIN->session;

	/* Check additional connections. */
	for (l = FST_PLUGIN->sessions; l; l = l->next)
	{
		if (((FSTSession*)l->data)->tcpcon->host == ip)
			return l->data;
	}

	return NULL;
}

/*****************************************************************************/
