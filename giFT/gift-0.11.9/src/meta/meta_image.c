/*
 * $Id: meta_image.c,v 1.5 2004/11/12 00:06:11 jasta Exp $
 *
 * Modified from <http://www.acme.com/software/image_size/>.  Original
 * license follows:
 *
 * image_size - figure out the image size of GIF, JPEG, XBM, or PNG files
 *
 * Copyright (C)1997,1998 by Jef Poskanzer <jef@acme.com>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "giftd.h"

#include "plugin/share.h"

#include "meta.h"
#include "meta_image.h"

/*****************************************************************************/

/* Define a few JPEG markers. */
#define M_SOF0 0xc0
#define M_SOF3 0xc3
#define M_SOI 0xd8
#define M_EOI 0xd9
#define M_SOS 0xda

/* Forwards. */
static BOOL image_size (FILE* f, int* widthP, int* heightP);
static BOOL gif (FILE* f, int* widthP, int* heightP);
static BOOL jpeg (FILE* f, int* widthP, int* heightP);
static BOOL png (FILE* f, int* widthP, int* heightP);
static BOOL get (FILE* f, unsigned char* chP);
static BOOL mustbe (FILE* f, unsigned char ch);

BOOL
image_size (FILE* f, int* widthP, int* heightP)
{
	unsigned char ch1, ch2;
	
	if (!get (f, &ch1)) return FALSE;
	if (!get (f, &ch2)) return FALSE;
	if (ch1 == 'G' && ch2 == 'I')
		return gif (f, widthP, heightP);
	else if (ch1 == 0xff && ch2 == M_SOI)
		return jpeg (f, widthP, heightP);
	else if (ch1 == 137 && ch2 == 80)
		return png (f, widthP, heightP);
	else
		return FALSE;
}

static BOOL
gif (FILE* f, int* widthP, int* heightP)
{
	unsigned char ch, w1, w2, h1, h2;

	/* Read rest of signature. */
	if (!mustbe(f, 'F')) return FALSE;
	if (!mustbe(f, '8')) return FALSE;
	if (!get(f, &ch)) return FALSE;
	if (ch != '7' && ch != '9')
		return FALSE;
	if (!mustbe(f, 'a')) return FALSE;

	/* Width and height are the next things in the file. */
	if (!get(f, &w1)) return FALSE;
	if (!get(f, &w2)) return FALSE;
	if (!get(f, &h1)) return FALSE;
	if (!get(f, &h2)) return FALSE;
	*widthP = (int) w2 * 256 + (int) w1;
	*heightP = (int) h2 * 256 + (int) h1;
	return TRUE;
}

static BOOL
jpeg (FILE* f, int* widthP, int* heightP)
{
	unsigned char ch, l1, l2, w1, w2, h1, h2;
	int l, i;

	/* JPEG blocks consist of a 0xff, a marker byte, a block size
	 * (two bytes, big-endian), and the rest of the block.  The
	 * block size includes itself - i.e. is the block size was 2 then
	 * there would be no additional bytes in the block.
	 *
	 * So, what we do here is read blocks until we get an SOF0-SOF3 marker,
	 * and then extract the width and height from that.
	 */
	for (;;)
	{
		if (!mustbe(f, 0xff)) return FALSE;
		if (!get(f, &ch)) return FALSE;
		if (!get(f, &l1)) return FALSE;
		if (!get(f, &l2)) return FALSE;
		l = (int) l1 * 256 + (int) l2;
		/* Is it the block we're looking for? */
		if (ch >= M_SOF0 && ch <= M_SOF3)
		{
			/* Toss the sample precision. */
			if (!get(f, &ch)) return FALSE;
			/* Read the height and width. */
			if (!get(f, &h1)) return FALSE;
			if (!get(f, &h2)) return FALSE;
			if (!get(f, &w1)) return FALSE;
			if (!get(f, &w2)) return FALSE;
			*widthP = (int) w1 * 256 + (int) w2;
			*heightP = (int) h1 * 256 + (int) h2;
			return TRUE;
		}
		if (ch == M_SOS || ch == M_EOI)
			return FALSE;
		/* Not the block we want.  Read and toss. */
		for (i = 2; i < l; ++i)
			if (!get(f, &ch)) return FALSE;
	}
}

static BOOL
png (FILE* f, int* widthP, int* heightP)
{
	unsigned char ch1, ch2, ch3, ch4, l1, l2, l3, l4, w1, w2, w3, w4, h1, h2, h3, h4;
	long l, i;
	
	/* Read rest of signature. */
	if (!mustbe(f, 78)) return FALSE;
	if (!mustbe(f, 71)) return FALSE;
	if (!mustbe(f, 13)) return FALSE;
	if (!mustbe(f, 10)) return FALSE;
	if (!mustbe(f, 26)) return FALSE;
	if (!mustbe(f, 10)) return FALSE;
	
	/* PNG chunks consist of a length, a chunk type, chunk data, and
	 * a CRC.  We read chunks until we get an IHDR chunk, and then
	 * extract the width and height from that.  Actually, the IHDR chunk
	 * is required to come first, but we might as well allow for
	 * broken encoders that don't obey that.
	 */
	for (;;)
	{
		if (!get(f, &l1)) return FALSE;
		if (!get(f, &l2)) return FALSE;
		if (!get(f, &l3)) return FALSE;
		if (!get(f, &l4)) return FALSE;
		l = (long) l1 * 16777216 + (long) l2 * 65536 + (long) l3 * 256 + (long) l4;
		if (!get(f, &ch1)) return FALSE;
		if (!get(f, &ch2)) return FALSE;
		if (!get(f, &ch3)) return FALSE;
		if (!get(f, &ch4)) return FALSE;
		/* Is it the chunk we're looking for? */
		if (ch1 == 'I' && ch2 == 'H' && ch3 == 'D' && ch4 == 'R')
		{
			/* Read the height and width. */
			if (!get(f, &w1)) return FALSE;
			if (!get(f, &w2)) return FALSE;
			if (!get(f, &w3)) return FALSE;
			if (!get(f, &w4)) return FALSE;
			if (!get(f, &h1)) return FALSE;
			if (!get(f, &h2)) return FALSE;
			if (!get(f, &h3)) return FALSE;
			if (!get(f, &h4)) return FALSE;
			*widthP = (long) w1 * 16777216 + (long) w2 * 65536 + (long) w3 * 256 + (long) w4;
			*heightP = (long) h1 * 16777216 + (long) h2 * 65536 + (long) h3 * 256 + (long) h4;
			return TRUE;
		}
		/* Not the block we want.  Read and toss. */
		for (i = 0; i < l + 4; ++i)
			if (!get (f, &ch1)) return FALSE;
	}
	return FALSE;
}

static BOOL
get (FILE* f, unsigned char* chP)
{
	int ich = getc (f);
	if (ich == EOF)
		return FALSE;

	*chP = (unsigned char) ich;

	return TRUE;
}

static BOOL
mustbe (FILE* f, unsigned char ch)
{
	unsigned char ch2;

	if (!get (f, &ch2)) 
		return FALSE;

	if (ch2 != ch)
		return FALSE;

	return TRUE;
}

/*****************************************************************************/

BOOL meta_image_run (Share *share, const char *path)
{
	int width, height;
	BOOL ret;
	FILE *f = fopen (path, "rb");

	if (!f)
		return FALSE;

	if ((ret = image_size (f, &width, &height)))
	{
		share_set_meta (share, "width",  stringf ("%d", width));
		share_set_meta (share, "height", stringf ("%d", height));
	}

	fclose (f);

	return ret;
}
