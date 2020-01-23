/*
 * $Id: meta_mpeg.c,v 1.6 2004/06/09 16:33:22 mkern Exp $
 *
 * This file was original contributed by np_ <napalm@arnet.com.ar>.
 *
 * Major rewrite by the giFT project developers.  Original ownership of the
 * code (that is, where np_ yanked it from) is unknown.
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

#include "meta.h"
#include "meta_mpeg.h"

#include <math.h>

/*****************************************************************************/

/*
 * I never wanted to get to know MPEG. You refactor if you want.
 * 
 * References:
 *
 * http://flavor.sourceforge.net/samples/mpeg1sys.htm (MPEG-1 Systems Layer)
 * http://www.wotsit.org/download.asp?f=mpeg2-1       (MPEG-2 Systems Layer)
 * http://www.wotsit.org/download.asp?f=mpeg2-2       (MPEG-2 Video)
 */

/*****************************************************************************/

#if 0
# define PARSE_DEBUG(x) GIFT_DBG(x)
#else
# define PARSE_DEBUG(x)
#endif

/*****************************************************************************/

/* 
 * The maximum number of bytes we scan for packet headers from the beginning
 * of the file. If we don't find all required headers we abort parsing.
 */
#define PACKET_SCAN_RANGE (128 * 1024)

/* 
 * The last byte of these is the stream id and the first 3 bytes are the
 * start code prefix.
 */
#define PACK_START_CODE          0x000001BA /* start of pack header */
#define SYSTEM_START_CODE        0x000001BB /* start of system header */
#define PROGRAM_END_CODE         0x000001B9 /* end of PES */
#define SEQUENCE_HEADER_CODE     0x000001B3 /* start of sequence header */
#define EXTENSION_START_CODE     0x000001B5 /* start of extension */
#define USER_DATA_START_CODE     0x000001B2 /* start of user data */
#define UNKNOWN_START_CODE_1     0x000001E0 /* couldn't find what this is */

#define START_CODE_PREFIX        0x00000100

/*****************************************************************************/

typedef struct
{
	unsigned int version;
	unsigned int width;                 /* width in pixels */
	unsigned int height;                /* height in pixels */
	double       fps;                   /* frames per second */
	unsigned int duration;              /* duration in milliseconds */
	unsigned int bitrate;               /* total bitrate in kbit/sec */
	unsigned int video_bitrate;         /* video bitrate in kbit/sec */
} MpegData;

/*****************************************************************************/

static double round_double (double num)
{
	return (floor (num + 0.5));
}

static uint32_t fread_be (FILE *file, int bytes)
{
	int x;
	uint32_t result = 0;

	assert (bytes <= sizeof (result));

	for (x = bytes - 1; x >= 0; x--)
		result |= (getc(file) << (x*8));

	return result;
}

/*****************************************************************************/

static int parse_mpeg (FILE *file, MpegData *data)
{
	int i, c;
	uint32_t temp;

	temp = fread_be (file, 4);

	/* mpegs may have leading zeros, skip past them */
	i = 0;
	while (temp != PACK_START_CODE)
	{
		if ((c = fgetc (file)) < 0 || (i++) >= PACKET_SCAN_RANGE)
		{
			GIFT_DBG (("No pack header found in MPEG stream"));
			return FALSE;
		}

		temp = (temp << 8) | (c & 0xFF);
	}

	PARSE_DEBUG (("Found first pack header at offset %d", i));

	/* figure out if this is an MPEG-1 or MPEG-2 */
	if ((c = fgetc (file)) < 0)
		return FALSE;

	if ((c & 0xF0) == 0x20)
	{
		data->version = 1;
		PARSE_DEBUG (("MPEG-1 stream"));
	}
	else if ((c & 0xC0) == 0x40)
	{
		data->version = 2;
		PARSE_DEBUG (("MPEG-2 stream"));
	}
	else
	{
		GIFT_DBG (("Unknown MPEG stream type %d", c));
		return FALSE;
	}
 
	/* read total stream bitrate */
	if(data->version == 1)
	{
		/* skip uninteresting parts of pack_header */
		fseek (file, 4, SEEK_CUR);

		/* 22 bits mux_rate in 50 bytes/second */
		temp = (fread_be (file, 3) & 0x7FFFFE) >> 1;
		data->bitrate = (unsigned int)(round_double ((double)(temp) * 0.4));
	}
	else /* version == 2 always */
	{
		/* skip uninteresting parts of pack_header */
		fseek (file, 5, SEEK_CUR);

		/* 22 bits program_mux_rate in 50 bytes/second */
		temp = (fread_be (file, 3) & 0xFFFFFC) >> 2;
		data->bitrate = (unsigned int)(round_double ((double)(temp) * 0.4));

		/* 3 bits packet_stuffing_length, skip stuffing bytes */
		if ((temp = fgetc (file) & 0x07) != 0)
			fseek (file, (long) temp, SEEK_CUR);
	}

	i = ftell (file);
	PARSE_DEBUG (("Looking for video sequence starting from %d", i));

	/* skip all headers until we find a video sequence */
	for (;;)
	{
		temp = fread_be (file, 4);
		i += 4;

		/* skip to next start code */
		while ((temp & 0xFFFFFF00) != START_CODE_PREFIX)
		{
			if ((c = fgetc (file)) < 0 || (i++) >= PACKET_SCAN_RANGE)
			{
				GIFT_DBG (("Too much padding in MPEG stream at %d", i));
				return FALSE;
			}

			temp = (temp << 8) | (c & 0xFF);
		}

		/* bail out if we find this */
		if (temp == PROGRAM_END_CODE)
		{
			GIFT_DBG (("No video sequence header found in MPEG stream (%d)", i));
			return FALSE;
		}

		/* break if we found the video sequence header */
		if (temp == SEQUENCE_HEADER_CODE)
			break;

		/* the end of user data is the next start code prefix */
		if (temp == USER_DATA_START_CODE)
			continue;

		/* no idea what this is but we need to skip it */
		if (temp == UNKNOWN_START_CODE_1)
			continue;

		if (temp == PACK_START_CODE)
		{
			/*
			 * Start of new pack header, skip the header. I hadn't expected this
			 * to become so messy, *sigh*.
			 */		
			PARSE_DEBUG (("Found pack header at offset %d", i));

			c = fgetc (file);
			i++;

			if ((c & 0xF0) == 0x20)
			{
				/* version 1 */
				fseek (file, 7, SEEK_CUR);
				i += 7;
			}
			else if ((c & 0xC0) == 0x40)
			{
				/* version 2 */
				fseek (file, 8, SEEK_CUR);

				/* 3 bits packet_stuffing_length, skip stuffing bytes */
				if ((temp = fgetc (file) & 0x07) != 0)
					fseek (file, (long) temp, SEEK_CUR);

				i += temp + 8;
			}
			else
			{
				GIFT_DBG (("Unknown MPEG stream type %d", c));
				return FALSE;
			}
		}
		else
		{
			/* other uninteresting header, skip it */
			temp = fread_be (file, 2);
			fseek (file, (long)temp, SEEK_CUR);
			i += temp + 2;
		}

		if (feof (file) || i >= PACKET_SCAN_RANGE)
		{
			GIFT_DBG (("Unexpected EOF in MPEG stream at %d", i));
			return FALSE;
		}
	}

	assert (temp == SEQUENCE_HEADER_CODE);
	PARSE_DEBUG (("Found video sequence header at offset %d", i));

	/* 12 bits per horizontal_size_value and vertical_size_value */
	temp = fread_be (file, 3);
	data->width = (temp & 0xFFF000) >> 12;
	data->height = temp & 0x000FFF;

	/* 4 bits frame_rate_code */
	switch (fgetc (file) & 0x0F)
	{
	 case 1:  data->fps = 23.976;  break;
	 case 2:  data->fps = 24.000;  break;
	 case 3:  data->fps = 25.000;  break;
	 case 4:  data->fps = 29.970;  break;
	 case 5:  data->fps = 30.000;  break;
	 case 6:  data->fps = 50.000;  break;
	 case 7:  data->fps = 59.940;  break;
	 case 8:  data->fps = 60.000;  break;
	}

	/* 18 bit bit_rate_value in 400 bits/second */
	temp = (fread_be (file, 3) & 0xFFFFC0) >> 6;
	if (temp != 0x3FFFF) /* variable bitrate */
		data->video_bitrate = (unsigned int)(round_double ((double)temp * 0.4));
	else
		data->video_bitrate = 0;

	/* skip other things in video sequence header */
	temp = fgetc (file);

	if (temp & 0x02)
	{
		fseek (file, 63, SEEK_CUR);
		temp = fgetc (file);
	}

	if (temp & 0x01)
		fseek (file, 64, SEEK_CUR);

	if (feof (file))
	{
		GIFT_DBG (("Unexpected EOF in MPEG stream"));
		return FALSE;
	}

	/* read MPEG-2 extension data */
	if (data->version == 2)
	{
		PARSE_DEBUG (("Parsing MPEG-2 extension at offset %d", ftell (file)));

		temp = fread_be (file, 4);
		if (temp == EXTENSION_START_CODE)
		{
			/* skip profile_and_level_indication */
			fseek (file, 1, SEEK_CUR);

			/* 2 bits per horizontal_size_extension and vertical_size_extension */
			temp = fread_be (file, 2);
			data->width  |= (temp & 0x0180) << 5;
			data->height |= (temp & 0x0060) << 7;

			/* TODO: 12 bits bit_rate_extension */
			fseek (file, 2, SEEK_CUR);

			/*
			 * 2 bits frame_rate_extension_n (numerator)
			 * 5 bits frame_rate_extension_d (denumerator)
			 */
			temp = fgetc (file);
			if ((temp & 0x60) && (temp & 0x1F))
				data->fps = (unsigned int)(round_double (
				            (double)data->fps * (temp & 0x60)/(temp & 0x1F)));
		}
	}

	/* calculate duration from file size and bitrate */
	data->duration = 0;
	if (data->bitrate != 0)
	{
		fseek (file, 0, SEEK_END);
		data->duration = (unsigned int)(round_double(
		                 (double)ftell (file) / data->bitrate * 8));
	}

	return TRUE;
}

/*****************************************************************************/

static BOOL video_file_analyze (Share *share, const char *path)
{
	FILE *file;
	char fmt[10];
	MpegData data;

	memset (&data, 0, sizeof (data));

	if (!(file = fopen (path, "rb")))
		return FALSE;

	PARSE_DEBUG (("Analyzing MPEG file (%s)", path));

	if (parse_mpeg (file, &data) == FALSE)
	{
		fclose (file);
		GIFT_DBG (("Invalid MPEG file (%s)", path));
		return FALSE;
	}

	fclose (file);

	switch (data.version)
	{
	 case 1:  strcpy (fmt, "MPEG-1");  break;
	 case 2:  strcpy (fmt, "MPEG-2");  break;
	 default: fmt[0] = 0;              break;
	}

	if (data.width > 0)
		share_set_meta (share, "width", stringf ("%u", data.width));

	if (data.height > 0)
		share_set_meta (share, "height", stringf ("%u", data.height));

	if (data.duration > 0)
		share_set_meta (share, "duration", stringf ("%u", data.duration / 1000));

	if (data.bitrate > 0)
		share_set_meta (share, "bitrate", stringf ("%u", data.bitrate * 1000));

	if (data.video_bitrate > 0)
		share_set_meta (share, "videobitrate", stringf ("%u", data.video_bitrate * 1000));

	/* i don't think these make much sense until we agree on key names */
#if 0
	if (fmt[0])
		share_set_meta (share, "format", fmt);

	if (data.fps > 0)
		share_set_meta (share, "fps", stringf ("%.03f", data.fps));
#endif

	return TRUE;
}

/*****************************************************************************/

BOOL meta_mpeg_run (Share *share, const char *path)
{
	return video_file_analyze (share, path);
}
