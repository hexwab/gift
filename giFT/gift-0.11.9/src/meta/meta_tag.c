/*
 * $Id: meta_tag.c,v 1.31 2004/06/11 08:34:29 hipnod Exp $
 *
 * Copyright (C) 20003 giFT project (gift.sourceforge.net)
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

#include "giftd.h"

#include "plugin/share.h"

#include "lib/file.h"

#include "meta.h"
#include "meta_tag.h"

/*****************************************************************************/

/*
 * This file parses id3v1 and id3v2 tags.
 *
 * TODO:
 *		- parse 16-bit Unicode strings and convert those that are
 *		  really just big ASCII strings (or is that ISO-8859-1?)
 *		- compressed frames; compressed metaframes (CDM)
 *
 * If anyone wants to do these things, please feel free.
 */

/*****************************************************************************/

#define ID3V2_HDR_LEN                10
#define ID3V2_FOOTER_LEN             10

#define ID3V2_EXT_HDR_LEN            6
#define ID3V2_FRAME_HDR_MAX          10

/* maximum amount of memory used in reading frames */
#define DEFAULT_MAX_FRAME_LEN        131072

/* maximum sane possible size of a frame: 28-bits */
#define MAX_FRAME_SIZE               0x0fffffff

/* improve readability */
#define ID3_DATA(tag)                (tag)->id3

/*****************************************************************************/

/* #define DEBUG_META_TAG 1 */

#ifdef DEBUG_META_TAG
#define META_TAG_TRACE(args)         GIFT_TRACE(args)
#else
#define META_TAG_TRACE(args)
#endif

/*****************************************************************************/

struct id3_data
{
	FILE              *f;
	char              *name;
	Dataset           *keys;
	struct id3v1      *id3v1;
	struct id3v2      *id3v2;
	Share             *share;
};

struct id3v1
{
	struct id3_data *id3;
};

struct id3v2
{
	struct id3_data *id3;
	size_t           max_len;            /* max buffer size */

	size_t           data_len;
	size_t           ext_hdr_len;        /* length of the ext hdr */
	size_t           size;               /* data_len + hdrs */
	size_t           data_offset;        /* offset in file */
	size_t           fake_offset;        /* equal to data_offset except when
	                                      * tag is unsynchronized */
	BOOL             unsynched;          /* whole tag unsynch-encoded */
	BOOL             compressed;
	BOOL             has_ext_hdr;        /* has extended header */
	BOOL             experimental;
	BOOL             has_footer;         /* has a 10-byte footer */

	unsigned int     version;
};

struct id3v2_frame
{
	struct id3v2      *tag;           /* tag this frame belongs to */
	char               id[5];

	uint8_t           *contents;
	size_t             size;
	size_t             offset;

	BOOL               unsynched;
	BOOL               compressed;
	BOOL               encrypted;
	BOOL               grouped;       /* grouped with previous tag */
	BOOL               has_data_len;  /* has data length indicator */
};

typedef struct frame_handler
{
	char     v2_str[4];              /* 3 char id from id3v2.2  */
	char     v3_str[5];              /* 4 char id from id3v2.3+ */

	/* callback for handling each frame */
	void   (*parse) (struct id3v2 *tag, struct id3v2_frame *frame);
} frame_handler_t;

/*****************************************************************************/

static void read_v2_frames   (struct id3v2 *tag);
static void read_v1          (struct id3v1 *tag);

static void set_meta         (struct id3_data *id3, char *id, char *key,
                              char *value);

/*****************************************************************************/

#define v2_frame_handler(name) \
	static void name (struct id3v2 *tag, struct id3v2_frame *frame)

v2_frame_handler (id3v2_parse_TLEN);
v2_frame_handler (id3v2_parse_COMM);
v2_frame_handler (id3v2_parse_TIT2);
v2_frame_handler (id3v2_parse_TALB);
v2_frame_handler (id3v2_parse_TRCK);
v2_frame_handler (id3v2_parse_TPE1);
v2_frame_handler (id3v2_parse_TYER);

/*****************************************************************************/

/* list of handled id3v2 frames */
static frame_handler_t frame_handlers[] =
{
	{ "TLE", "TLEN", id3v2_parse_TLEN }, /* duration */
	{ "COM", "COMM", id3v2_parse_COMM }, /* comment */
	{ "TT2", "TIT2", id3v2_parse_TIT2 }, /* title */
	{ "TAL", "TALB", id3v2_parse_TALB }, /* album */
	{ "TRK", "TRCK", id3v2_parse_TRCK }, /* track */
	{ "TP1", "TPE1", id3v2_parse_TPE1 }, /* artist */
	{ "TYE", "TYER", id3v2_parse_TYER }, /* year */
	{ "", "", NULL }
};

/*****************************************************************************/
/* ID3v1 support */

static struct id3v1 *id3v1_alloc (struct id3_data *id3)
{
	struct id3v1 *tag;

	if (!(tag = MALLOC (sizeof (struct id3v1))))
		return NULL;

	tag->id3 = id3;
	return tag;
}

static void id3v1_free (struct id3v1 *tag)
{
	if (!tag)
		return;

	if (ID3_DATA(tag) && ID3_DATA(tag)->id3v1 == tag)
		ID3_DATA(tag)->id3v1 = NULL;

	free (tag);
}

static char *v1_str (unsigned char *p, size_t len)
{
	static char   v1_buf[64];
	size_t        i;

	for (i = 0; i < len; i++)
	{
		unsigned char c = p[i];

		/* check if it was terminated, and i think 0xff is not valid */
		if (c == 0 || c == 0xff)
			break;

		v1_buf[i] = p[i];
	}

	v1_buf[i] = 0;
	string_trim (v1_buf);

	return v1_buf;
}

/* The ID3v1 tag is always 128 bytes long and somewhere before
 * the end of the file */
static void read_v1 (struct id3v1 *tag)
{
	char            buf[512];
	unsigned char  *p;
	size_t          comment_len;
	char           *str;

	assert (ID3_DATA(tag)->f != NULL);

	if (fseek (ID3_DATA(tag)->f, -128L, SEEK_END) != 0)
		return;

	if (fread (buf, 1, 128, ID3_DATA(tag)->f) != 128)
		return;

	if (strncmp (buf, "TAG", 3) != 0)
		return;

	p = buf + 3;

	set_meta (ID3_DATA(tag), NULL, "title",   v1_str (p, 30)); p += 30;
	set_meta (ID3_DATA(tag), NULL, "artist",  v1_str (p, 30)); p += 30;
	set_meta (ID3_DATA(tag), NULL, "album",   v1_str (p, 30)); p += 30;
	set_meta (ID3_DATA(tag), NULL, "year",    v1_str (p, 4));  p += 4;

	/* check for track number, which is indicated by having byte
	 * 29 of the comment field nulled (and a sane track number) */
	if (p[28] == 0 && p[29] > 0 && p[29] != 0xff)
		comment_len = 29;
	else
		comment_len = 30;

	set_meta (ID3_DATA(tag), NULL, "comment", v1_str (p, comment_len));
	p += comment_len;

	if (comment_len == 29)
	{
		str = stringf_dup ("%d", *p);
		set_meta (ID3_DATA(tag), NULL, "track", str);
		free (str);
		p++;
	}

	/* TODO: genre table? */
}

/*****************************************************************************/
/* ID3v2 support */

struct id3v2 *id3v2_alloc (struct id3_data *id3, size_t max_len)
{
	struct id3v2 *tag;

	if (!(tag = MALLOC (sizeof (struct id3v2))))
		return NULL;

	tag->id3 = id3;

	/* set the maximum frame size to read at any one time */
	if (max_len == 0)
		max_len = DEFAULT_MAX_FRAME_LEN;

	tag->max_len = max_len;
	return tag;
}

void id3v2_free (struct id3v2 *tag)
{
	if (!tag)
		return;

	if (ID3_DATA(tag) && ID3_DATA(tag)->id3v2 == tag)
		ID3_DATA(tag)->id3v2 = NULL;

	/* frames aren't stored, so there's nothing else to free */
	free (tag);
}

static struct id3v2_frame *id3v2_frame_alloc (struct id3v2 *tag, const char *id)
{
	struct id3v2_frame *frame;

	if (!(frame = MALLOC (sizeof (struct id3v2_frame))))
		return NULL;

	frame->tag      = tag;
	frame->contents = NULL;
	frame->size     = 0;
	frame->offset   = 0;

	strcpy (frame->id, id);

	return frame;
}

static void id3v2_frame_free (struct id3v2_frame *frame)
{
	if (!frame)
		return;

	free (frame->contents);
	free (frame);
}

static int id3v2_major_version (struct id3v2 *tag)
{
	return (tag->version & 0xff00) >> 8;
}

#ifdef DEBUG_META_TAG /* XXX */
static int id3v2_minor_version (struct id3v2 *tag)
{
	return (tag->version & 0x00ff);
}
#endif

/*****************************************************************************/

/* get bytes in big-endian ordering */
static uint32_t get_bytes (unsigned char **p, size_t len)
{
	int             i;
	uint32_t        bytes;
	unsigned char  *ptr;

	assert (len <= 4);
	bytes = 0;
	ptr   = *p;

	for (i = 0; i < len; i++)
		bytes = (bytes << 8) | (*ptr++);

	*p = ptr;
	return bytes;
}

/*
 * Read 'len' "sync-safe" bytes, that were encoded by making
 * sure the leading bit of each byte is 0.
 */
static size_t get_sync_safe (unsigned char **ptr, size_t len)
{
	size_t          decoded;
	unsigned char  *p;

	assert (len == 4);
	p = *ptr;

	decoded = ((p[0] & 0x7f) << 21) | ((p[1] & 0x7f) << 14) |
	          ((p[2] & 0x7f) << 7)  | ((p[3] & 0x7f));

	*ptr += len;
	return decoded;
}

/*
 * Read some data from the tag, reverse any unsynchronization replacing
 * sequences of [ff 00] with [ff], and copy the data to a buffer.
 *
 * NOTE: This method doesn't unsync sequences straddling the boundaries of
 * reads, so each id3v2_resync() _must_ completely encompass all possible
 * unsynchronized sequences within a given subrange of the tag. Otherwise
 * the tag will not be read correctly.
 */
static size_t id3v2_resync (struct id3v2 *tag, char *buf, size_t len)
{
	int     c;
	int     cprev;
	size_t  n;

	if (tag->data_offset >= tag->size)
		return 0;

	n     = 0;
	cprev = 0;

	while (tag->data_offset < tag->size && n < len)
	{
		if ((c = getc (ID3_DATA(tag)->f)) == EOF)
			break;

		tag->data_offset++;

		/* unsync 0xff00 sequence to 0xff (and also, 0xff0000 to 0xff00) */
		if (cprev == 0xff && c == 0x00)
		{
			cprev = 0;
			continue;
		}

		/* we read a byte */
		n++;

		if (buf)
			buf[n-1] = c;

		if (n >= len)
			break;

		cprev = c;
	}

	/* update the logical position in the tag */
	tag->fake_offset += n;

	return n;
}

/* read some data from the tag, possibly unsyncing it, into the supplied
 * buffer and update tag->data_offset */
static size_t id3v2_read (struct id3v2 *tag, void *buf, size_t need_len)
{
	size_t unsynced_len;

	if (!tag->unsynched || id3v2_major_version (tag) >= 4)
	{
		/*
		 * If the tag does not need unsynchronization, or if it
		 * needs unsync but is version 4, we can just do fread().
		 */
		unsynced_len = fread (buf, 1, need_len, ID3_DATA(tag)->f);

		tag->data_offset += unsynced_len;
		tag->fake_offset += unsynced_len;

		return unsynced_len;
	}

	/*
	 * The tag needs unsynchronization, and we've got to do it the
	 * hard way: one character at a time.
	 *
	 * tag->data_offset is handled by id3v2_resync().
	 */
	unsynced_len = id3v2_resync (tag, (char *)buf, need_len);
	return unsynced_len;
}

/*
 * Seek to a particular spot in the tag.
 *
 * If the tag is unsynchronized and the id3 major version is 2 or 3, we
 * have to fake doing a seek by doing id3v2_read(). We do this because in
 * those versions of the standard, all sizes in the tag except the tag size
 * are _logical_ sizes, and the real sizes can only be obtained after
 * unsyncing the whole tag.
 */
static int id3v2_seek (struct id3v2 *tag, size_t offset, int whence)
{
	size_t ret;
	size_t len;

	assert (whence == SEEK_SET);

	if (!tag->unsynched || id3v2_major_version (tag) >= 4)
	{
		if (offset > LONG_MAX)
			return -1;

		if (fseek (ID3_DATA(tag)->f, (long)offset, SEEK_SET) != 0)
			return -1;

		/* update real and virtual offsets, which are the same when the tag is
		 * not unsynchronized */
		tag->data_offset = offset;
		tag->fake_offset = offset;

		return 0;
	}

	/* can't seek backwards */
	if (tag->fake_offset > offset)
		return -1;

	len = offset - tag->fake_offset;
	ret = id3v2_read (tag, NULL, len);

	if (ret != len)
		return -1;

	return 0;
}

/* UNTESTED */
static BOOL skip_ext_header (struct id3v2 *tag, size_t start)
{
	unsigned char    buf[ID3V2_EXT_HDR_LEN];
	unsigned char   *p;
	size_t           newpos;

	if (!ID3_DATA(tag)->f)
		return FALSE;

	if (id3v2_seek (tag, start, SEEK_SET) != 0)
		return FALSE;

	if (id3v2_read (tag, buf, ID3V2_EXT_HDR_LEN) != ID3V2_EXT_HDR_LEN)
		return FALSE;

	p = buf;

	/*
	 * v2.2.0-2.3.0: header size is 4 byte int
	 * v2.4.0: header size is sync-safe
	 */
	if (id3v2_major_version (tag) >= 4)
		tag->ext_hdr_len = get_sync_safe (&p, 4);
	else
		tag->ext_hdr_len = get_bytes (&p, 4);

	/* the extended header can't be too big */
	if (tag->ext_hdr_len > MAX_FRAME_SIZE)
		return FALSE;

	newpos = start + tag->ext_hdr_len;

	/*
	 * Sanity check the extended header size: if we have unsynchronize
	 * the header, make sure it isn't humongous. We don't want to fgetc()
	 * on 20MB, say.
	 */
	if (tag->unsynched && id3v2_major_version (tag) < 4)
	{
		if (tag->ext_hdr_len > tag->max_len)
		    return FALSE;
	}

	/*
	 * Skip the header, as it doesn't have any frames we care
	 * about at the moment (crc32, update flag, ...)
	 */
	if (id3v2_seek (tag, newpos, SEEK_SET) != 0)
		return FALSE;

	return TRUE;
}

static BOOL unknown_tag_flags (struct id3v2 *tag, int flags)
{
	assert (id3v2_major_version (tag) >= 2 && id3v2_major_version (tag) <= 4);

	/* v2: xx000000. bit 5 represents some weird compression scheme
	 * using a CDM frame that is not supported. */
	if (id3v2_major_version (tag) == 2)
		return (flags & 0x3f) != 0;

	/* v3: xxx00000 */
	if (id3v2_major_version (tag) == 3)
		return (flags & 0x1f) != 0;

	/* v4: xxxx0000 */
	return (flags & 0x0f) != 0;
}

static void set_tag_flags (struct id3v2 *tag, int flags)
{
	tag->unsynched    = FALSE;
	tag->compressed   = FALSE;
	tag->has_ext_hdr  = FALSE;
	tag->experimental = FALSE;
	tag->has_footer   = FALSE;

	tag->unsynched = (flags & 0x80) != 0;

	if (id3v2_major_version (tag) == 2)
		tag->compressed = (flags & 0x40) != 0;
	else
		tag->has_ext_hdr = (flags & 0x40) != 0;

	tag->experimental = (flags & 0x20) != 0;
	tag->has_footer   = (flags & 0x10) != 0;
}

static BOOL parse_v2_header (struct id3v2 *tag, unsigned char *hdr,
                             char *signature)
{
	int              i;
	int              not_found;
	unsigned char   *h;
	size_t           sig_len;
	unsigned char    flags;

	h = hdr;
	not_found = 0;

	sig_len = strlen (signature);

	/* sanity check the values in the header */
	not_found = strncmp (h, signature, sig_len);
	h += sig_len;

	/* {major, minor} version: should _not_ equal 0xff */
	for (i = 0; i < 2; i++)
		not_found |= *h++ == 0xff;

	/* flags: must have none of the lower 4 bits set (according to
	 * version 4 of the standard) */
	not_found |= (*h++ & 0xf);

	/* the size of the tag: must have highest bit clear */
	for (i = 0; i < 4; i++)
		not_found |= (*h++ & 0x80);

	if (not_found)
		return FALSE;

	META_TAG_TRACE (("---: [%s] found \"%s\"\n", ID3_DATA(tag)->name,
	                 signature));

	h = hdr + sig_len;

	/* get the version field */
	tag->version = get_bytes (&h, 2);

	META_TAG_TRACE (("version = ID3v2.%u.%u\n", id3v2_major_version (tag),
	                 id3v2_minor_version (tag)));

	/* support v2.2.0-v2.4.x */
	if (id3v2_major_version (tag) < 2 || id3v2_major_version (tag) > 4)
		return FALSE;

	flags = *h++;

	/* if the tag has any weird flags set, bail */
	if (unknown_tag_flags (tag, flags))
	{
		META_TAG_TRACE (("unknown tag flags: %x\n", flags));
		return FALSE;
	}

	set_tag_flags (tag, flags);

	META_TAG_TRACE (("[%s]: unsync_tag: %i extended: %i experimental: %i "
	                 "has_footer: %i\n", ID3_DATA(tag)->name, tag->unsynched,
	                 tag->has_ext_hdr, tag->experimental, tag->has_footer));

	/*
	 * The length of the whole tag is encoded as a "sync-safe"
	 * integer, where the highest bit of each byte is clear, yielding
	 * a 28-bit integer with a max tag length of 256 MB.
	 */
	tag->data_len = get_sync_safe (&h, 4);
	tag->size = tag->data_len + ID3V2_HDR_LEN;

	if (tag->has_footer)
		tag->size += ID3V2_FOOTER_LEN;

	return TRUE;
}

static void read_v2 (struct id3v2 *tag, char *signature)
{
	unsigned char    header[ID3V2_HDR_LEN];

	assert (ID3_DATA(tag)->f != NULL);

	if (fseek (ID3_DATA(tag)->f, 0, SEEK_SET) != 0)
		return;

	if (fread (header, 1, ID3V2_HDR_LEN, ID3_DATA(tag)->f) != ID3V2_HDR_LEN)
		return;

	if (!parse_v2_header (tag, header, signature))
		return;

	/* put the real and virtual offsets after the header */
	tag->data_offset = ID3V2_HDR_LEN;
	tag->fake_offset = ID3V2_HDR_LEN;

	/* can't handle compressed tags yet */
	if (tag->compressed)
		return;

	META_TAG_TRACE (("taglength=%u (0x%x)\n", tag->data_len, tag->data_len));

	if (tag->has_ext_hdr && !skip_ext_header (tag, ID3V2_HDR_LEN))
		return;

	read_v2_frames (tag);
}

/*****************************************************************************/
/* Frame reading */

static size_t frame_hdr_len (struct id3v2 *tag)
{
	if (id3v2_major_version (tag) == 2)
		return 6;

	return ID3V2_FRAME_HDR_MAX;
}

static size_t frame_id_len (struct id3v2 *tag)
{
	if (id3v2_major_version (tag) == 2)
		return 3;

	return 4;
}

static BOOL is_frameid_char (int c)
{
	if (c >= 65 /*'A'*/ && c <= 90 /*'Z'*/)
		return TRUE;
	else if (c >= 48 /*'0'*/ && c <= 57 /*'9'*/)
		return TRUE;

	return FALSE;
}

static uint8_t unknown_frame_flags (struct id3v2 *tag, int flags1, int flags2)
{
	assert (id3v2_major_version (tag) >= 2 &&
	        id3v2_major_version (tag) <= 4);

	if (id3v2_major_version (tag) == 2)
		return 0;

	/* v2.3.0: 11100000 11100000 where 1's are significant */
	if (id3v2_major_version (tag) == 3)
		return (flags1 & 0x1f) | (flags2 & 0x1f);

	/* v2.4.0: 01110000 01001111, changed to avoid false framesyncs */
	return (flags1 & 0x8f) | (flags2 & 0xb0);
}

static void set_frame_flags (struct id3v2_frame *frame, int flags1, int flags2)
{
	struct id3v2 *tag = frame->tag;

	assert (id3v2_major_version (tag) >= 2 &&
	        id3v2_major_version (tag) <= 4);

	/* v2.2.0: no flags */
	if (id3v2_major_version (tag) == 2)
		return;

	if (id3v2_major_version (tag) == 3)
	{
		frame->compressed = (flags2 & 0x80) != 0;
		frame->encrypted  = (flags2 & 0x40) != 0;
		frame->grouped    = (flags2 & 0x20) != 0;
		return;
	}

	frame->grouped      = (flags2 & 0x40) != 0;
	frame->compressed   = (flags2 & 0x08) != 0;
	frame->encrypted    = (flags2 & 0x04) != 0;
	frame->unsynched    = (flags2 & 0x02) != 0;
	frame->has_data_len = (flags2 & 0x01) != 0;
}

static BOOL frame_alloc_buffer (struct id3v2_frame *frame, size_t len)
{
	uint8_t *buf;

	assert (len > 0);
	assert (len <= MAX_FRAME_SIZE + 1);
	assert (frame->contents == NULL);

	if (!(buf = malloc (len)))
		return FALSE;

	frame->size     = len;
	frame->contents = buf;

	return TRUE;
}

static frame_handler_t *find_handler (frame_handler_t *p, char *id)
{
	if (!id)
		return NULL;

	while (p->parse != NULL)
	{
		if (!STRCMP (id, p->v2_str) || !STRCMP (id, p->v3_str))
			return p;

		p++;
	}

	return NULL;
}

/* read the frame headers and store their offsets in the file */
static BOOL read_frame (struct id3v2 *tag, struct id3v2_frame *frame,
                        frame_handler_t *handler, size_t len)
{
	size_t n;

	/* can't handle compressed or encrypted frames */
	if (!handler || frame->encrypted || frame->compressed)
		return FALSE;

	/* check if this frame would exceed memory allotment */
	if (len > tag->max_len)
	{
		META_TAG_TRACE (("%s EXCEEDS max len (%u > %u)\n",
		                 frame->id, len, tag->max_len));
		return FALSE;
	}

	if (!frame_alloc_buffer (frame, len + 1))
		return FALSE;

	/*
	 * Version 4 of the standard allows for avoiding all the unsync
	 * problems, but we have to unsync the frame contents here
	 * instead of dynamically in id3v2_read().
	 */
	if ((tag->unsynched || frame->unsynched) && id3v2_major_version (tag) >= 4)
		n = id3v2_resync (tag, frame->contents, len);
	else
		n = id3v2_read (tag, frame->contents, len);

	if (n != len)
	{
		META_TAG_TRACE (("FREAD failed: %d returned (%d)\n", n, len));
		return FALSE;
	}

	/* null terminate the buffer */
	frame->contents[len] = 0;

	/* call the handler */
	handler->parse (tag, frame);

	return TRUE;
}

static size_t get_frame_len (struct id3v2 *tag, unsigned char *p)
{
	size_t data_len;

	/*
	 * v2.2.0 - 3 bytes length field
	 * v2.3.0 - 4 bytes ""
	 * v2.4.0 - sync-safe
	 */
	switch (id3v2_major_version (tag))
	{
	 case 2: data_len = get_bytes     (&p, 3); break;
	 case 3: data_len = get_bytes     (&p, 4); break;
	 case 4: data_len = get_sync_safe (&p, 4); break;
	 default: abort ();
	}

	return data_len;
}

static void read_v2_frames (struct id3v2 *tag)
{
	unsigned char        buf[ID3V2_FRAME_HDR_MAX];
	unsigned char        frame_id[5];
	size_t               id_len;
	size_t               hdr_len;
	size_t               pos;
	struct id3v2_frame  *frame;

	if (!ID3_DATA(tag)->f)
		return;

	id_len  = frame_id_len  (tag);
	hdr_len = frame_hdr_len (tag);

	META_TAG_TRACE (("frame data size=%lu\n",
	                 (long)tag->size - tag->data_offset));

	/* get the current logical position in the tag */
	pos = tag->fake_offset;

	while (tag->data_offset < tag->size)
	{
		unsigned char    *p;
		size_t            data_len;
		int               i;
		frame_handler_t  *handler;

		if (id3v2_seek (tag, pos, SEEK_SET) != 0)
			return;

		if (tag->data_offset >= tag->size)
			break;

		/* read the frame header */
		if (id3v2_read (tag, buf, hdr_len) != hdr_len)
			return;

		memcpy (frame_id, buf, id_len);
		frame_id[id_len] = 0;

		/* check for a frame id (possibly padding) without ascii characters */
		for (i = 0; i < id_len; i++)
		{
			if (!is_frameid_char (frame_id[i]))
			    return;
		}

		p = buf + id_len;
		data_len = get_frame_len (tag, p);

		/* sanity check the length */
		if (data_len > MAX_FRAME_SIZE)
			break;

		/*
		 * Could try to continue parsing here. The most likely occurrence
		 * though is that the tag is corrupt and we're in the middle of
		 * garbage.
		 */
		if (unknown_frame_flags (tag, buf[8], buf[9]))
		{
			META_TAG_TRACE (("FUCK! bad flags: %x %x\n", buf[8], buf[9]));
			break;
		}

		/* increment the logical position in the tag */
		pos += hdr_len + data_len;

		META_TAG_TRACE (("[%d bytes left]\n", tag->size - tag->data_offset));

		frame = id3v2_frame_alloc (tag, frame_id);
		set_frame_flags (frame, buf[8], buf[9]);

		META_TAG_TRACE (("###: %s LEN: %u [%lu]\n", frame_id, data_len,
		                 ftell (ID3_DATA(tag)->f)));

		META_TAG_TRACE (("%s: group: %i compress: %i encrypt: %i "
		                 "unsync: %i data_len: %i\n", frame->id,
		                 frame->grouped, frame->compressed, frame->encrypted,
		                 frame->unsynched, frame->has_data_len));

		handler = find_handler (frame_handlers, frame_id);

		read_frame (tag, frame, handler, data_len);
		id3v2_frame_free (frame);
	}
}

/*****************************************************************************/
/* Frame parsing */

static uint8_t id3v2_frame_get_uint8 (struct id3v2_frame *frame)
{
	if (frame->offset >= frame->size)
		return 0;

	return frame->contents[frame->offset++];
}

static char *id3v2_frame_get_str (struct id3v2_frame *frame)
{
	char *p;

	if (frame->offset >= frame->size)
		return NULL;

	p = &frame->contents[frame->offset];

	/* the frame gets null-terminated when read, so this should be safe */
	frame->offset += STRLEN_0 (p);

	return p;
}

static char *id3v2_frame_get_encoded_str (struct id3v2_frame *frame)
{
	uint8_t encoding = id3v2_frame_get_uint8 (frame);

	/*
	 * Only support ISO-8859-1 encoding, which is marked with zero byte in the
	 * "encoding" byte.
	 *
	 * TODO: This is wrong, as only UTF-8 metadata should exist from giftd's
	 * and each plugin's point of view.
	 */
	if (encoding != 0)
		return NULL;

	return id3v2_frame_get_str (frame);
}

static BOOL is_lang_char (int c)
{
	if (c >= 97 /*'a'*/ && c <= 122 /*'z'*/)
		return TRUE;

	return FALSE;
}

static char *id3v2_frame_get_lang (struct id3v2_frame *frame)
{
	static char lang[4];
	int         i;
	size_t      end;

	end = MIN (frame->offset + 3, frame->size);

	/* check overflow */
	if (frame->offset + 3 < frame->offset)
		end = frame->size;

	for (i = 0; i < 3; i++)
	{
		if (frame->offset >= frame->size)
			return NULL;

		if (!(lang[i] = id3v2_frame_get_uint8 (frame)))
		    break;

		if (!is_lang_char (lang[i]))
			break;
	}

	lang[i] = 0;

	/* skip the three language characters */
	frame->offset = end;

	return lang;
}

/*****************************************************************************/
/* Individual ID3v2 frame handling */

static void id3v2_parse_TLEN (struct id3v2 *tag, struct id3v2_frame *frame)
{
	unsigned int millis;
	int          n;
	char        *str;
	char         buf[128];

	if (!(str = id3v2_frame_get_encoded_str (frame)))
		return;

	META_TAG_TRACE (("TLEN: contents=%s [len=%d]\n", str, frame->size));

	if ((n = sscanf (str, "%u", &millis)) != 1)
		return;

	snprintf (buf, sizeof (buf) - 1, "%u", millis / 1000);
	set_meta (ID3_DATA(tag), frame->id, "duration", buf);
}

static void id3v2_parse_COMM (struct id3v2 *tag, struct id3v2_frame *frame)
{
	uint8_t encoding = id3v2_frame_get_uint8 (frame);
	char   *lang     = id3v2_frame_get_lang  (frame);
	char   *descr    = id3v2_frame_get_str   (frame);

	/* ISO-8859-1 == 0 */
	if (encoding != 0)
		return;

	META_TAG_TRACE (("COMM: lang=%s description=%s\n", lang, descr));
	lang = descr; /* prevent 'unused variable' warnings */

	set_meta (ID3_DATA(tag), frame->id, "comment",
	          id3v2_frame_get_str (frame));
}

static void id3v2_parse_TIT2 (struct id3v2 *tag, struct id3v2_frame *frame)
{
	set_meta (ID3_DATA(tag), frame->id, "title",
	          id3v2_frame_get_encoded_str (frame));
}

static void id3v2_parse_TALB (struct id3v2 *tag, struct id3v2_frame *frame)
{
	set_meta (ID3_DATA(tag), frame->id, "album",
	          id3v2_frame_get_encoded_str (frame));
}

static void id3v2_parse_TRCK (struct id3v2 *tag, struct id3v2_frame *frame)
{
	set_meta (ID3_DATA(tag), frame->id, "track",
	          id3v2_frame_get_encoded_str (frame));
}

static void id3v2_parse_TPE1 (struct id3v2 *tag, struct id3v2_frame *frame)
{
	set_meta (ID3_DATA(tag), frame->id, "artist",
	          id3v2_frame_get_encoded_str (frame));
}

static void id3v2_parse_TYER (struct id3v2 *tag, struct id3v2_frame *frame)
{
	set_meta (ID3_DATA(tag), frame->id, "year",
	          id3v2_frame_get_encoded_str (frame));
}

/*****************************************************************************/

static void id3_data_close (struct id3_data *id3)
{
	if (!id3)
		return;

	free (id3->name);

	if (id3->f)
	{
		fclose (id3->f);
		id3->f = NULL;
	}

	id3v2_free (id3->id3v2);
	id3v1_free (id3->id3v1);

	dataset_clear (id3->keys);
	id3->keys = NULL;

	free (id3);
}

static struct id3_data *id3_data_open (Share *share, const char *path,
                                       size_t max_len)
{
	struct id3_data *id3;

	if (!(id3 = malloc (sizeof (struct id3_data))))
		return NULL;

	id3->f     = NULL;
	id3->name  = STRDUP (path);
	id3->keys  = NULL;
	id3->share = share;

	id3->id3v1 = id3v1_alloc (id3);
	id3->id3v2 = id3v2_alloc (id3, max_len);

	if (!id3->id3v1 || !id3->id3v2)
	{
		id3_data_close (id3);
		return NULL;
	}

	if (!(id3->f = fopen (path, "rb")))
	{
		id3_data_close (id3);
		return NULL;
	}

	return id3;
}

static void id3_data_read (struct id3_data *id3)
{
	read_v1 (id3->id3v1);
	read_v2 (id3->id3v2, "ID3");
}

/*****************************************************************************/

/* set a meta key read from the file */
static void set_meta (struct id3_data *id3, char *id, char *key, char *val)
{
	char   *old;
	char   *max_str;
	size_t  max_len;
	size_t  len;

	if (!key || !val)
		return;

	/* only handle certain keys */
	if (!(max_str = dataset_lookupstr (id3->keys, key)))
		return;

	max_len = ATOUL (max_str);

	/* trim the data */
	string_trim (val);

	old = share_get_meta (id3->share, key);
	len = strlen (val);

	/* if the old id was larger, take that one instead */
	if (old && strlen (old) > len)
		return;

	/* don't set empty meta-information */
	if (string_isempty (val))
		return;

	/* cap the data at 128 chars */
	if (len > max_len)
		val[max_len] = 0;

	/* if id was supplied this is id3v2 metainfo */
	if (id)
		META_TAG_TRACE (("=== %s (%s): %s\n", id, key, val));
	else
		META_TAG_TRACE (("=== [v1] (%s): %s\n", key, val));

	share_set_meta (id3->share, key, val);
}

static void add_key (struct id3_data *id3, char *key, size_t max_len)
{
	char *str;

	if (!(str = stringf_dup ("%lu", (unsigned long)max_len)))
		return;

	dataset_insertstr (&id3->keys, key, str);
	free (str);
}

void meta_tag_run (Share *share, const char *path)
{
	struct id3_data *id3;

	if (!(id3 = id3_data_open (share, path, DEFAULT_MAX_FRAME_LEN)))
		return;

	/* insert the keys we are interested in, as well as the max length
	 * for each key */
	add_key (id3, "duration", 128);
	add_key (id3, "comment",  128);
	add_key (id3, "title",    128);
	add_key (id3, "album",    128);
	add_key (id3, "track",    10);
	add_key (id3, "artist",   128);
	add_key (id3, "year",     6);

	id3_data_read (id3);
	id3_data_close (id3);
}

/*****************************************************************************/

#if 0
int main (int argc, char **argv)
{
	struct id3_data *tags;
	Share            share;
	int              i;

	for (i = 1; i < argc; i++)
	{
		printf ("checking %s\n", argv[i]);
		share_init (&share);

		if (!(tags = id3_data_open (&share, argv[i], 0)))
			continue;

		id3_data_read (tags);
		id3_data_close (tags);

		share_finish (&share);
	}

	return 0;
}
#endif
