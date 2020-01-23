/*
 * $Id: meta_avi.c,v 1.9 2004/06/21 22:31:11 jasta Exp $
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

#include "giftd.h"

#include "plugin/share.h"

#include "lib/file.h"

/*****************************************************************************/

#if 0
# define PARSE_DEBUG(x) GIFT_DBG(x)
#else
# define PARSE_DEBUG(x)
#endif

/*
 * References:
 *
 * http://www.jmcgowan.com/avitech.html#Format
 * http://msdn.microsoft.com/library/default.asp?url=/library/en-us/
 *        directx9_c/directx/htm/avirifffilereference.asp
 */

/*****************************************************************************/

/*
 * Maximum number of streams we read.  Currently only the first video and
 * audio streams are used. Since there is always only one video stream
 * reading a total of two streams is enough.
 */
#define AVI_MAX_STREAMS 2

/*****************************************************************************/

/* all fields in a RIFF file are little endian */
#ifdef WORDS_BIGENDIAN
# define SWAP32(x) (((x & 0x000000FF) << 24) | \
                    ((x & 0x0000FF00) << 8)  | \
                    ((x & 0x00FF0000) >> 8)  | \
                    ((x & 0xFF000000) >> 24))
# define SWAP16(x) (((x & 0x00FF) << 8) | \
                    ((x & 0xFF00) >> 8))
#else
# define SWAP32(x) (x)
# define SWAP16(x) (x)
#endif

/* create little endian uint32_t from chars */
#define FOURCC(a,b,c,d) ((((uint32_t)d) << 24) | \
                         (((uint32_t)c) << 16) | \
                         (((uint32_t)b) << 8)  | \
                          ((uint32_t)a))

/*****************************************************************************/

typedef struct
{
	uint32_t magic;  /* FOURCC('R','I','F','F') */
	uint32_t len;
	uint32_t id;     /* FOURCC('A','V','I',' ') */
} RIFFHeader;

typedef struct
{
	uint32_t id;
	uint32_t len;
} ChunkHeader;

typedef struct
{
	uint32_t magic;  /* FOURCC('L','I','S','T') */
	uint32_t len;
	uint32_t id;
} ListHeader;

typedef struct
{
	uint32_t micro_sec_per_frame;
	uint32_t max_bytes_per_sec;
	uint32_t padding_granularity;
	uint32_t flags;
	uint32_t total_frames;
	uint32_t initial_frames;
	uint32_t streams;
	uint32_t suggested_buffer_size;
	uint32_t width;
	uint32_t height;
	uint32_t reserved[4];
} AviMainHeader;

typedef struct
{
	uint32_t type;
	uint32_t handler;
	uint32_t flags;
	uint16_t priority;
	uint16_t language;
	uint32_t initial_frames;
	uint32_t scale;
	uint32_t rate;
	uint32_t start;
	uint32_t length;
	uint32_t suggested_buffer_size;
	uint32_t quality;
	uint32_t sample_size;
	struct
	{
		uint16_t left;
		uint16_t top;
		uint16_t right;
		uint16_t bottom;
	}  frame;
} AviStreamHeader;

/*****************************************************************************/

typedef struct
{
	unsigned int width;             /* width in pixels */
	unsigned int height;            /* height in pixels */
	double       fps;               /* video frames per second */
	unsigned int duration;          /* duration in milliseconds */
	unsigned int bitrate;           /* total bitrate in kbit/sec */
	unsigned int video_bitrate;     /* video bitrate in kbit/sec */
	unsigned int audio_bitrate;     /* audio bitrate in kbit/sec */
	char video_codec[5];            /* human readable video codec of 4 chars */
	char audio_codec[5];            /* human readable audio codec of 4 chars */
} AviData;

/*****************************************************************************/

static void init_avidata (AviData *data)
{
	data->width          = 0;
	data->height         = 0;
	data->duration       = 0;
	data->fps            = 0;
	data->video_bitrate  = 0;
	data->audio_bitrate  = 0;
	data->video_codec[0] = '\0';
	data->audio_codec[0] = '\0';
}

/*****************************************************************************/

static BOOL read_riff (RIFFHeader *header, FILE *file)
{
	if (fread (header, sizeof (RIFFHeader), 1, file) != 1)
		return FALSE;

	header->magic = SWAP32(header->magic);
	header->len   = SWAP32(header->len);
	header->id    = SWAP32(header->id);

	if (header->magic != FOURCC('R','I','F','F') ||
	    header->id    != FOURCC('A','V','I',' '))
		return FALSE;

	return TRUE;
}

static BOOL read_chunk (ChunkHeader *header, FILE *file)
{
	if (fread (header, sizeof (ChunkHeader), 1, file) != 1)
		return FALSE;

	header->len = SWAP32(header->len);
	header->id  = SWAP32(header->id);

	return TRUE;
}

static BOOL read_list (ListHeader *header, FILE *file)
{
	if (fread (header, sizeof (ListHeader), 1, file) != 1)
		return FALSE;

	header->magic = SWAP32(header->magic);
	header->len   = SWAP32(header->len);
	header->id    = SWAP32(header->id);

	if (header->magic != FOURCC('L','I','S','T'))
		return FALSE;

	return TRUE;
}

static BOOL read_stream (AviStreamHeader *stream_header, FILE *file)
{
	ChunkHeader chunk;
	ListHeader list;
	long list_end;

	assert (stream_header != NULL);

	/* read stream list */
	if (read_list (&list, file) == FALSE ||
	    list.id != FOURCC('s','t','r','l'))
	{
		GIFT_DBG (("Expected stream list not found in AVI"));
		return FALSE;
	}

	list_end = ftell (file) - sizeof (list.id) + list.len;

	/* read stream header chunk */
	if (read_chunk (&chunk, file) == FALSE ||
	    chunk.id != FOURCC('s','t','r','h'))
	{
		GIFT_DBG (("Expected stream header chunk not found in AVI"));
		return FALSE;
	}

	/* read actual stream header */
	if (fread (stream_header, sizeof (AviStreamHeader), 1, file) != 1)
	{
		GIFT_DBG (("Expected stream header not found in AVI"));
		return FALSE;
	}

	/* skip rest of this stream list */
	if (fseek (file, list_end, SEEK_SET) != 0)
	{
		GIFT_DBG (("Unexpected EOF when reading stream in AVI"));
		return FALSE;
	}

	return TRUE;
}

/*****************************************************************************/

static BOOL parse_avi (FILE *file, AviData *data)
{
	RIFFHeader      riff;
	ChunkHeader     chunk;
	ListHeader      list;
	AviMainHeader   main_header;
	AviStreamHeader streams[AVI_MAX_STREAMS];
	unsigned int    i;

	/* read riff header */
	if (read_riff (&riff, file) == FALSE)
	{
		GIFT_DBG (("No RIFF header found in AVI"));
		return FALSE;
	}

	/* read header list */
	if (read_list (&list, file) == FALSE ||
	    list.id != FOURCC('h','d','r','l'))
	{
		GIFT_DBG (("No header list found in AVI"));
		return FALSE;
	}

	/* get avi main header chunk */
	if (read_chunk (&chunk, file) == FALSE ||
	    chunk.id != FOURCC('a','v','i','h'))
	{
		GIFT_DBG (("No avi main header chunk found in AVI"));
		return FALSE;
	}

	/* read actual main header */
	if (fread (&main_header, sizeof(AviMainHeader), 1, file) != 1)
	{
		GIFT_DBG (("No avi main header found in AVI"));
		return FALSE;
	}

	PARSE_DEBUG (("%u streams in AVI", SWAP32(main_header.streams)));

	/* read stream headers */
	for (i = 0; i < SWAP32(main_header.streams) && i < AVI_MAX_STREAMS; i++)
	{
		if (read_stream (&streams[i], file) == FALSE)
			return FALSE;
	}

	PARSE_DEBUG (("Read %u streams from AVI", i));

	/* the video stream always comes first */
	if (SWAP32(main_header.streams) < 1 ||
	    SWAP32(streams[0].type) != FOURCC('v','i','d','s'))
	{
		GIFT_DBG (("First stream not video in AVI"));
		return FALSE;
	}

	/* get video codec */
	memcpy (data->video_codec, (char*)&streams[0].handler, 4);
	data->video_codec[4] = 0;

	/* get video bitrate in byte/sec */
	data->video_bitrate = 0;

	if (SWAP32(streams[0].rate) != 0 && SWAP32(streams[0].scale) != 0)
	{
		PARSE_DEBUG (("Calculating video fps"));
		data->fps = ((double)SWAP32(streams[0].rate)) /
		                     SWAP32(streams[0].scale);

		if (SWAP32(streams[0].sample_size) != 0)
		{
			PARSE_DEBUG (("Calculating video bitrate"));
			data->video_bitrate = data->fps *
			                      SWAP32(streams[0].sample_size) / 125;
		}
	}

	/* there is only one video stream so audio stream is next if present */
	data->audio_codec[0] = 0;
	data->audio_bitrate  = 0;

	if (SWAP32(main_header.streams) >= 2 && AVI_MAX_STREAMS >= 2 &&
	    SWAP32(streams[1].type) == FOURCC('a','u','d','s'))
	{
		PARSE_DEBUG (("Found audio stream in AVI"));

#if 0
		/*
		 * This is not working as expected.We should probably get audio codec
		 * from stream format chunk.
		 */

		/* get audio codec */
		memcpy (data->audio_codec, (char *)&streams[1].handler, 4);
		data->audio_codec[4] = 0;
#endif

		/* get audio bitrate in byte/sec */
		data->video_bitrate = 0;
		if (SWAP32(streams[1].sample_size) != 0 &&
		    SWAP32(streams[1].rate)        != 0 &&
		    SWAP32(streams[1].scale)       != 0)
		{
			PARSE_DEBUG (("Calculating audio bitrate"));
			data->audio_bitrate = ((double)SWAP32(streams[1].rate)) /
			                      SWAP32(streams[1].scale) *
			                      SWAP32(streams[1].sample_size) / 125;
		}
	}

	/* get height and width from main header */
	data->width  = SWAP32(main_header.width);
	data->height = SWAP32(main_header.height);

	/* get total bitrate */
	fseek (file, 0, SEEK_END);
	data->bitrate = ((double)ftell (file)) /
	                SWAP32(main_header.total_frames) * data->fps / 125;

	/* get play length */
	data->duration = ((double)SWAP32(main_header.micro_sec_per_frame)) /
	                   1000 * SWAP32(main_header.total_frames);

	PARSE_DEBUG (("AVI analysis complete"));

	return TRUE;
}

/*****************************************************************************/

BOOL meta_avi_run (Share *share, const char *path)
{
	FILE   *file;
	AviData data;
	char    codec[16]; /* 16 chars is always enough */

	PARSE_DEBUG (("Analyzing AVI file (%s)", path));

	if (!(file = fopen (path, "rb")))
	{
		GIFT_DBG (("Unable to open AVI (%s) for analysis", path));
		return FALSE;
	}

	init_avidata (&data);

	if (parse_avi (file, &data) == FALSE)
	{
		fclose (file);
		GIFT_DBG (("Invalid AVI file (%s)", path));
		return FALSE;
	}

	fclose (file);

	share_set_meta (share, "width",    stringf ("%u", data.width));
	share_set_meta (share, "height",   stringf ("%u", data.height));
	share_set_meta (share, "duration", stringf ("%u", data.duration / 1000));

#if 0
	if (data.fps > 0)
		share_set_meta (share, "fps", stringf ("%.03f", data.fps));
#endif

	if (data.video_bitrate > 0)
		share_set_meta (share, "videobitrate", stringf ("%u", data.video_bitrate * 1000));

	if (data.audio_bitrate > 0)
		share_set_meta (share, "audiobitrate", stringf ("%u", data.audio_bitrate * 1000));

	if (data.bitrate > 0)
		share_set_meta (share, "bitrate", stringf ("%u", data.bitrate * 1000));

	if (data.video_codec[0] != '\0')
	{
		size_t vclen;

		if ((vclen = strlen (data.video_codec)) < sizeof (codec))
		{
			strcpy (codec, data.video_codec);

			if (data.audio_codec[0] != '\0')
			{
				snprintf (codec + vclen, sizeof (codec) - vclen, " / %s",
				          data.audio_codec);
			}

			share_set_meta (share, "codec", codec);
		}
	}

	return TRUE;
}

