/*
 * meta_jpeg.c - from IJG's rdjpgcom.c
 *
 * Copyright (C) 1994-1997, Thomas G. Lane.
 * This file is part of the Independent JPEG Group's software.
 * For conditions of distribution and use, see the accompanying README file.
 *
 */

#include "meta.h"

static FILE *fh;

#define BYTE(var) { int byte=fgetc(fh); if (byte==EOF) return 0; var=byte; }
#define WORD(var) { int j,k; BYTE(j); BYTE(k); var=(j<<8)+k; }

/*
 * JPEG markers consist of one or more 0xFF bytes, followed by a marker
 * code byte (which is not an FF).  Here are the marker codes of interest
 * in this program.  (See jdmarker.c for a more complete list.)
 */

#define M_SOF0  0xC0		/* Start Of Frame N */
#define M_SOF1  0xC1		/* N indicates which compression process */
#define M_SOF2  0xC2		/* Only SOF0-SOF2 are now in common use */
#define M_SOF3  0xC3
#define M_SOF5  0xC5		/* NB: codes C4 and CC are NOT SOF markers */
#define M_SOF6  0xC6
#define M_SOF7  0xC7
#define M_SOF9  0xC9
#define M_SOF10 0xCA
#define M_SOF11 0xCB
#define M_SOF13 0xCD
#define M_SOF14 0xCE
#define M_SOF15 0xCF
#define M_SOI   0xD8		/* Start Of Image (beginning of datastream) */
#define M_EOI   0xD9		/* End Of Image (end of datastream) */
#define M_SOS   0xDA		/* Start Of Scan (begins compressed data) */
#define M_APP0	0xE0		/* Application-specific marker, type N */
#define M_APP12	0xEC		/* (we don't bother to list all 16 APPn's) */
#define M_COM   0xFE		/* COMment */


/*
 * Most types of marker are followed by a variable-length parameter segment.
 * This routine skips over the parameters for any marker we don't otherwise
 * want to process.
 * Note that we MUST skip the parameter segment explicitly in order not to
 * be fooled by 0xFF bytes that might appear within the parameter segment;
 * such bytes do NOT introduce new markers.
 */

static int
skip_variable (FILE *fh)
/* Skip over an unknown or uninteresting variable-length marker */
{
	unsigned int length;

	/* Get the marker parameter length count */
	WORD (length);

	/* Length includes itself, so must be at least 2 */
	if (length < 2)
		return 0;

	length -= 2;

	/* Skip over the remaining bytes */
	if (fseek (fh, length, SEEK_CUR))
		return 0;

	return 1;
}


/*
 * Process a SOFn marker.
 * This code is only needed if you want to know the image dimensions...
 */

static int
process_SOFn (FILE *fh, FileShare *file)
{
	unsigned int length;
	unsigned int height, width;
	int data_precision, num_components;

	WORD (length);	/* usual parameter length count */

	BYTE (data_precision);
	WORD (height);
	WORD (width);
	BYTE (num_components);

	meta_set (file, "Width",   stringf ("%hu", bitrate));
	meta_set (file, "Height",   stringf ("%hu", bitrate));

	if (length != (unsigned int) (8 + num_components * 3))
		return 0;

	if (fseek (fh,num_components*3,SEEK_CUR))
		return 0;

	return 1;
}


static int
process_jpeg (FILE *fh, FileShare *file)
{
	fh = f;

	if (fgetc(fh)!=0xff)
		return 0;

	if (fgetc(fh)!=M_SOI)
		return 0;

	/* Scan miscellaneous markers until we reach SOS. */
	for (;;) {
		int c;

		/* Find 0xFF byte; count and skip any non-FFs. */
		BYTE(c);
		while (c != 0xFF) {
			BYTE(c);
		}

		/* Get marker code byte, swallowing any duplicate FF bytes.  Extra FFs
		 * are legal as pad bytes, so don't count them in discarded_bytes.
		 */
		do {
			BYTE(c);
		} while (c == 0xFF);
    
		switch (c) {
			/* Note that marker codes 0xC4, 0xC8, 0xCC are not, and must not be,
			 * treated as SOFn.  C4 in particular is actually DHT.
			 */
		case M_SOF0:		/* Baseline */
		case M_SOF1:		/* Extended sequential, Huffman */
		case M_SOF2:		/* Progressive, Huffman */
		case M_SOF3:		/* Lossless, Huffman */
		case M_SOF5:		/* Differential sequential, Huffman */
		case M_SOF6:		/* Differential progressive, Huffman */
		case M_SOF7:		/* Differential lossless, Huffman */
		case M_SOF9:		/* Extended sequential, arithmetic */
		case M_SOF10:		/* Progressive, arithmetic */
		case M_SOF11:		/* Lossless, arithmetic */
		case M_SOF13:		/* Differential sequential, arithmetic */
		case M_SOF14:		/* Differential progressive, arithmetic */
		case M_SOF15:		/* Differential lossless, arithmetic */
			if (!process_SOFn (fh, file))
				return 0;
			break;

		case M_SOS:
		case M_EOI:			/* in case it's a tables-only JPEG stream */
			return 1;

		default:			/* Anything else just gets skipped */
			if (!skip_variable (fh))
				return 0;		/* we assume it has a parameter count... */
			break;
		}
	}
}

int meta_jpeg_run (FileShare *file)
{
	FILE         *fh;
	int          valid;
	ft_uint32     hdr = 0;

	if (!(fh = fopen (SHARE_DATA(file)->path, "r")))
		return 0;

	valid = process_jpeg (fh, file);

	fclose (fh);

	return valid;
}