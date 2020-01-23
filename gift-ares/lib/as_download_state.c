/*
 * $Id: as_download_state.c,v 1.3 2004/12/19 18:34:08 mkern Exp $
 *
 * Copyright (C) 2004 Markus Kern <mkern@users.berlios.de>
 * Copyright (C) 2004 Tom Hargreaves <hex@freezone.co.uk>
 *
 * All rights reserved.
 */

#include "as_ares.h"

/*****************************************************************************/

#define ARESTRA_MAGIC "___ARESTRA__2"
#define ARESTRA_MAGIC_LEN 13

/*****************************************************************************/

static ASPacket *read_state (FILE *fp, size_t *current_filesize);
static as_bool write_state (FILE *fp, ASPacket *packet, size_t filesize);

static as_bool read_chunks (ASDownload *dl, ASPacket *packet,
                            size_t current_filesize);
static as_bool write_chunks (ASDownload *dl, ASPacket *packet);

static as_bool read_tlvs (ASDownload *dl, ASPacket *packet);
static as_bool write_tlvs (ASDownload *dl, ASPacket *packet);

/*****************************************************************************/

/* Load state data from file and update download. */
as_bool as_downstate_load (ASDownload *dl)
{
	ASPacket *packet;
	List *link;
	ASDownChunk *chunk;
	size_t current_filesize;
	as_uint8 u8;
	as_bool paused = FALSE;

	assert (dl->chunks == NULL);
	assert (dl->conns == NULL);
	assert (dl->fp);
	assert (dl->state == DOWNLOAD_NEW);

	/* Read state data */
	if (!(packet = read_state (dl->fp, &current_filesize)))
	{
		AS_ERR_1 ("Couldn't find state data in incomplete download \"%s\"",
		          dl->filename);
		return FALSE;
	}

	/* Init download */
	dl->size = as_packet_get_le32 (packet);
	dl->received = as_packet_get_le32 (packet);

	/* Read chunks */
	if (!read_chunks (dl, packet, current_filesize))
	{
		AS_ERR_1 ("Couldn't load chunks for incomplete download \"%s\"",
		          dl->filename);
		as_packet_free (packet);
		return FALSE;
	}

	/* Recalculate dl->received from complete chunks */
	dl->received = 0;
	for (link = dl->chunks; link; link = link->next)
	{
		chunk = link->data;
		if (chunk->received == chunk->size)
			dl->received += chunk->size;
	}

	if (as_packet_remaining (packet) < 16)
	{
		AS_ERR_1 ("Not enough state data remaining after reading chunks from "
		          "incomplete download \"%s\"", dl->filename);
		as_packet_free (packet);
		return FALSE;	
	}

	/* unknown */
	if ((u8 = as_packet_get_8 (packet)) != 0x01)
	{
		AS_WARN_2 ("Unknown flag is 0x%02X instead of expected 0x01 for "
		           "incomplete download \"%s\"", u8, dl->filename);
	}

	/* paused state */
	if ((u8 = as_packet_get_8 (packet)) == 0x01)
	{
		paused = TRUE;
	}
	else if (u8 != 0x00)
	{
		AS_WARN_2 ("Paused flag is 0x%02X instead of expected 0x00 or 0x01 "
		           "for incomplete download \"%s\"", u8, dl->filename);
	}

	/* unknown */
	as_packet_get_le32 (packet); /* 0x00000080 */
	as_packet_get_le32 (packet); /* 0x00000000 */
	as_packet_get_le32 (packet); /* 0x000000F8 */

	/* read and parse type-length-value triples */
	if (!read_tlvs (dl, packet))
	{
		AS_ERR_1 ("Couldn't read TLVs for incomplete download \"%s\"",
		          dl->filename);
		as_packet_free (packet);
		return FALSE;
	}

	/* Make sure we got a hash out of the TLVs. */
	if (!dl->hash)
	{
		AS_ERR_1 ("No hash in state data for download \"%s\"", dl->filename);
		as_packet_free (packet);
		return FALSE;
	}

	/* We are finished. */
	as_packet_free (packet);

	/* Set new state */
	if (paused)
		dl->state = DOWNLOAD_PAUSED;
	else
		dl->state = DOWNLOAD_ACTIVE;

	return TRUE;
}

/* Save state data to end of file being downloaded. */
as_bool as_downstate_save (ASDownload *dl)
{
	ASPacket *packet;
	size_t received;
	List *link;
	ASDownChunk *chunk;

	assert (dl->chunks);
	assert (dl->fp);

	if (!(packet = as_packet_create ()))
		return FALSE;

	/* Assemble state data. */
	as_packet_put_le32 (packet, dl->size);

	/* Sum up complete chunks. */
	received = 0;
	for (link = dl->chunks; link; link = link->next)
	{
		chunk = link->data;
		if (chunk->received == chunk->size)
			received += chunk->size;
	}
	as_packet_put_le32 (packet, received);

	AS_HEAVY_DBG_3 ("Saving state. File size: %u, received: %u, complete chunks: %u",
	                dl->size, dl->received, received);

	/* Write chunks. */
	if (!write_chunks (dl, packet))
	{
		AS_ERR_1 ("Unable to assemble chunk data for incomplete download \"%s\"",
		          dl->filename);
		as_packet_free (packet);
		return FALSE;
	}

	/* Unknown */
	as_packet_put_8 (packet, 0x01);

	/* Paused flag */
	if (dl->state == DOWNLOAD_PAUSED)
		as_packet_put_8 (packet, 0x01);
	else
		as_packet_put_8 (packet, 0x00);

	/* Unknowns */
	as_packet_put_le32 (packet, 0x00000080);
	as_packet_put_le32 (packet, 0x00000000);
	as_packet_put_le32 (packet, 0x000000F8);

	/* Write TLVs. */
	if (!write_tlvs (dl, packet))
	{
		AS_ERR_1 ("Unable to assemble TLVs for incomplete download \"%s\"",
		          dl->filename);
		as_packet_free (packet);
		return FALSE;
	}

	/* Write data to disk. */
	if (!write_state (dl->fp, packet, dl->size))
	{
		AS_ERR_1 ("Couldn't write state to incomplete download \"%s\"",
		          dl->filename);
		as_packet_free (packet);
		return FALSE;
	}

	as_packet_free (packet);

	return TRUE;
}

/*****************************************************************************/

/* Returns packet with state data or NULL if it wasn't found. */
static ASPacket *read_state (FILE *fp, size_t *current_filesize)
{
	int i, len;
	long pos;
	as_uint8 buf[4096];
	ASPacket *packet;

	/* Search a maximum of 64k in 4k steps for magic. */
	for (i = 1; i <= 16; i++)
	{
		if (fseek (fp, -(i * 4096), SEEK_END) != 0)
			return NULL;

		if (fread (buf, ARESTRA_MAGIC_LEN, 1, fp) != 1)
			return NULL;

		if (memcmp (buf, ARESTRA_MAGIC, ARESTRA_MAGIC_LEN) == 0)
			break;
	}

	if (i > 16)
		return NULL;

	/* Save current media file size. */
	if ((pos = ftell (fp)) < 0)
		return FALSE;

	if (current_filesize)
		*current_filesize = pos - ARESTRA_MAGIC_LEN;

	/* Create packet to hold data. */
	if (!(packet = as_packet_create ()))
	{
		AS_ERR ("Insufficient memory.");
		return NULL;
	}

	/* Read state until end of file. */
	while ((len = fread (buf, 1, sizeof (buf), fp)) > 0)
	{
		if (!as_packet_put_ustr (packet, buf, len))
		{
			as_packet_free (packet);
			return NULL;
		}
	}

	/* We should have read at exactly i*4096-ARESTRA_MAGIC_LEN bytes  */
	if (as_packet_size (packet) != (unsigned int)i * 4096 - ARESTRA_MAGIC_LEN)
	{
		as_packet_free (packet);
		return NULL;
	}

	return packet;		
}

static as_bool write_state (FILE *fp, ASPacket *packet, size_t filesize)
{
	int len;

	/* Write state data after end of file. */
	if (fseek (fp, filesize, SEEK_SET) != 0)
	{
		AS_ERR ("Couldn't seek to file size.");
		return FALSE;
	}

	/* Write magic */
	if (fwrite (ARESTRA_MAGIC, ARESTRA_MAGIC_LEN, 1, fp) != 1)
	{
		AS_ERR ("Couldn't write magic.");
		return FALSE;
	}

	/* Pad packet with zeros to have a multiple of 4k length. */
	len = 4096 - ((as_packet_size (packet) + ARESTRA_MAGIC_LEN) % 4096);
	if (!as_packet_pad (packet, 0x00, len))
		return FALSE;	

	assert (((as_packet_size (packet) + ARESTRA_MAGIC_LEN) % 4096) == 0);

	/* Write entire packet to disk. */
	if (fwrite (packet->data, as_packet_size (packet), 1, fp) != 1)
	{
		AS_ERR ("Couldn't write state data.");
		return FALSE;
	}

	return TRUE;
}

/*****************************************************************************/

static as_bool read_chunks (ASDownload *dl, ASPacket *packet,
                            size_t current_filesize)
{
	as_uint32 start, end, size;
	List *l, *prev_l, *next_l;
	ASDownChunk *chunk, *prev_chunk, *next_chunk;

	assert (dl->chunks == NULL);

	/* The saved ranges are empty chunks up to current_filesize. So start out
	 * with a complete chunk up to current_filesize and an empty chunk from
	 * current_filsize to dl->filesize.
	 */

	/* complete chunk */
	if (!(chunk = as_downchunk_create (0, current_filesize)))
		return FALSE;
	chunk->received = chunk->size;
	dl->chunks = list_append (dl->chunks, chunk);

	/* empty chunk */
	if (!(chunk = as_downchunk_create (current_filesize,
	                                   dl->size - current_filesize)))
	{
		as_downchunk_free (dl->chunks->data);
		dl->chunks = list_free (dl->chunks);
		return FALSE;
	}
	dl->chunks = list_append (dl->chunks, chunk);

	/* Now cut out the still empty parts. */
	while (as_packet_remaining (packet) >= 8)
	{
		start = as_packet_get_le32 (packet);
		end   = as_packet_get_le32 (packet);

		if (start == 0 && end == 0)
		{
			/* End of ranges. */
			return TRUE;
		}

		if (end <= start ||
		    start >= current_filesize ||
		    end >= current_filesize)
		{
			break;
		}

		/* Range is inclusive */
		size = end - start + 1;

		/* Find the chunk which contains this range */
		for (prev_l = dl->chunks; prev_l; prev_l = prev_l->next)
		{
			prev_chunk = prev_l->data;
			if (start >= prev_chunk->start &&
			    start < prev_chunk->start + prev_chunk->size)
			{
				break;
			}
		}

		if (!prev_l)
			break;

		/* The found chunk must be complete */
		if (prev_chunk->received != prev_chunk->size)
			break;

		/* The ranges in the file must not overlap so the end of the range
		 * must fall in the same chunk.
		 */
		if (start + size > prev_chunk->start + prev_chunk->size)
			break;

		/* Create new chunk for range */
		if (!(chunk = as_downchunk_create (start, size)))
			break;

		/* Create new chunk to hold rest of previous chunk */
		if (!(next_chunk = as_downchunk_create (chunk->start + chunk->size,
		                        prev_chunk->start + prev_chunk->size - 
		                        chunk->start - chunk->size)))
		{
			break;
		}
		next_chunk->received = next_chunk->size;

		/* Shorten the previous chunk to start of new one */
		prev_chunk->size = chunk->start - prev_chunk->start;
		prev_chunk->received = prev_chunk->size;

		/* These must hold now independently from input data. */
		assert (prev_chunk->start + prev_chunk->size == chunk->start);
		assert (chunk->start + chunk->size == next_chunk->start);
		assert (prev_chunk->received == prev_chunk->size);
		assert (next_chunk->received == next_chunk->size);
		assert (chunk->received == 0);

		/* If the previous chunk has a positive size insert chunk after it
		 * otherwise replace it with chunk.
		 */
		if (prev_chunk->size > 0)
		{
			l = list_prepend (NULL, chunk);
			list_insert_link (prev_l, l);
		}
		else
		{
			as_downchunk_free (prev_chunk);
			prev_l->data = chunk;
			l = prev_l;
		}

		/* If the next chunk has a positive size insert it after chunk 
		 * otherwise drop it.
		 */
		if (next_chunk->size > 0)
		{
			next_l = list_prepend (NULL, next_chunk);
			list_insert_link (l, next_l);
		}
		else
		{
			as_downchunk_free (next_chunk);
		}
	}

	/* Something went wrong, free chunks. */
	for (l = dl->chunks; l; l = l->next)
		as_downchunk_free (l->data);
	dl->chunks = list_free (dl->chunks);

	return FALSE;
}

static as_bool write_chunks (ASDownload *dl, ASPacket *packet)
{
	ASDownChunk *chunk;
	List *link;

	assert (dl->chunks);

	/* The saved ranges are empty chunks up to current_filesize. We always
	 * use the entire file size and write the appendix after it. So just save
	 * all chunks which are not complete.
	 */
	for (link = dl->chunks; link; link = link->next)
	{
		chunk = link->data;

		/* Skip complete chunks. */
		if (chunk->received == chunk->size)
			continue;

		/* Write incomplete chunk. If download is not active this will be
		 * completely empty chunks.
		 */
		as_packet_put_le32 (packet, chunk->start);
		as_packet_put_le32 (packet, chunk->start + chunk->size - 1);
	}

	/* Write terminator. */
	as_packet_put_le32 (packet, 0);
	as_packet_put_le32 (packet, 0);

	return TRUE;
}

/*****************************************************************************/

static as_bool read_sources (ASDownload *dl, ASPacket *packet, as_uint16 len)
{
	ASSource *source;

	while (len >= 17)
	{
		/* Create a source for the data. */
		if (!(source = as_source_create ()))
			break;

		source->host  = as_packet_get_ip (packet);
		source->port  = as_packet_get_le16 (packet);
		source->shost = as_packet_get_ip (packet);
		source->sport = as_packet_get_le16 (packet);

		/* user's inside ip */
		as_packet_get_ip (packet);
		
		if (as_packet_get_8 (packet) != 0x00)
			break;
		len -= 17;

		/* Add source to download if not firewalled. */
		if (!as_source_firewalled (source))
		{
			/* Download state is still DOWNLOAD_NEW here so adding the source
			 * won't start downloading.
			 */
			assert (dl->state == DOWNLOAD_NEW);
			as_download_add_source (dl, source);
		}

		as_source_free (source);
	}

	/* Make sure any rest data is skipped */
	packet->read_ptr += len;

	return TRUE;
}

static as_bool read_tlvs (ASDownload *dl, ASPacket *packet)
{
	as_uint16 total_len, len;
	as_uint8 type;

	assert (dl->hash == NULL);

	/* get total length of all TLVs */
	total_len = as_packet_get_le16 (packet);

	if (as_packet_remaining (packet) < total_len)
		return FALSE;

	/* Loop over all TLVs */
	while (total_len > 0)
	{
		if (total_len < 3)
			return FALSE;

		type = as_packet_get_8 (packet);
		len  = as_packet_get_le16 (packet);
		total_len -= 3;

		if (total_len < len)
			return FALSE;

		/* Parse types. */
		switch (type)
		{
		case DOWNSTATE_TAG_SOURCES:
			read_sources (dl, packet, len);
			break;

		case DOWNSTATE_TAG_HASH:
			if (!dl->hash && len == AS_HASH_SIZE)
				dl->hash = as_packet_get_hash (packet);
			else
				packet->read_ptr += len;
			break;

		case DOWNSTATE_TAG_KEYWORDS:
		case DOWNSTATE_TAG_TITLE:
		case DOWNSTATE_TAG_ARTIST:
		case DOWNSTATE_TAG_ALBUM:
		case DOWNSTATE_TAG_GENRE:
		case DOWNSTATE_TAG_YEAR:
		case DOWNSTATE_TAG_COMMENT_1:
		case DOWNSTATE_TAG_COMMENT_2:
		case DOWNSTATE_TAG_UNKNOWN_1:
		case DOWNSTATE_TAG_UNKNOWN_2:
			/* Skip those until we add meta data to downloads. */
			packet->read_ptr += len;
			break;

		default:
			AS_WARN_3 ("Unknown TLV type 0x%02X of length %u in state data "
			           "for download \"%s\"", type, len, dl->filename);
			/* Skip these unknowns. */
			packet->read_ptr += len;
		}

		total_len -= len;
	}

	return TRUE;
}

/* Currently unused. */
#if 0
static void tlv_append_str (ASPacket *packet, ASDownStateTags type,
                            const char *str)
{
	as_uint16 len = strlen (str);

	/* type and length */
	as_packet_put_8 (packet, (as_uint8) type);
	as_packet_put_le16 (packet, (as_uint16) len);
	/* data */
	as_packet_put_ustr (packet, (as_uint8 *)str, len);
}
#endif

static as_bool tlv_append_sources (ASPacket *packet, ASDownload *dl)
{
	ASSource *source;
	List *link;
	ASPacket *body;

	if (!(body = as_packet_create ()))
		return FALSE;

	for (link = dl->conns; link; link = link->next)
	{
		source = ((ASDownConn *)link->data)->source;

		/* Ignore firewalled sources */
		if (as_source_firewalled (source))
			continue;

		/* Append source data */
		as_packet_put_ip (body, source->host);
		as_packet_put_le16 (body, source->port);
		as_packet_put_ip (body, source->shost);
		as_packet_put_le16 (body, source->sport);
		
		/* User's inside ip. */
		as_packet_put_ip (body, 0x00); 

		/* Terminator. */
		as_packet_put_8 (body, 0x00);	
	}

	/* Append type, length and body. */
	as_packet_put_8 (packet, DOWNSTATE_TAG_SOURCES);
	as_packet_put_le16 (packet, (as_uint16) as_packet_size (body));

	if (!as_packet_append (packet, body))
	{
		as_packet_free (body);
		return FALSE;
	}

	as_packet_free (body);

	return TRUE;
}

static as_bool write_tlvs (ASDownload *dl, ASPacket *packet)
{
	ASPacket *tlvs_packet;

	assert (dl->hash);

	if (!(tlvs_packet = as_packet_create ()))
		return FALSE;

	/* Add hash */
	as_packet_put_8 (tlvs_packet, DOWNSTATE_TAG_HASH);
	as_packet_put_le16 (tlvs_packet, AS_HASH_SIZE);
	as_packet_put_hash (tlvs_packet, dl->hash);

	/* Add sources */
	tlv_append_sources (tlvs_packet, dl);

	/* TODO: Add meta data */

	/* Append TLVs to packet */
	as_packet_put_le16 (packet, (as_uint16) as_packet_size (tlvs_packet));

	if (!as_packet_append (packet, tlvs_packet))
	{
		as_packet_free (tlvs_packet);
		return FALSE;
	}

	as_packet_free (tlvs_packet);

	return TRUE;
}

/*****************************************************************************/
