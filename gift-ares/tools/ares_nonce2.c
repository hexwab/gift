/*
 * $Id: ares_nonce2.c,v 1.1 2006/02/27 14:39:39 mkern Exp $
 *
 * Copyright (C) 2003 giFT-Ares project
 * http://developer.berlios.de/projects/gift-ares
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

/* Compile with msvc and tasm: 
 *   tasm32 /kh40000 /ml as_crypt_boring.asm
 *   cl ares_nonce2.c as_crypt_boring.obj
 */

#include <stdio.h>
#include <stdlib.h>

typedef unsigned int as_uint32;
typedef unsigned short as_uint16;
typedef unsigned char as_uint8;

#ifdef _MSC_VER
# define strcasecmp(s1,s2) _stricmp(s1, s2)
#endif

#define FATAL_ERROR(x) { fprintf (stderr, "\nFATAL: %s\n", x); exit (1); }

/*****************************************************************************/

#define SHA_BLOCKSIZE		64
#define SHA_DIGESTSIZE		20

#define SHA1_BINSIZE        20
#define SHA1_STRLEN         32

/*****************************************************************************/

typedef struct sha1_state_t
{
	unsigned long  digest[5];           /* message digest */
	unsigned long  count_lo, count_hi;  /* 64-bit bit count */
	as_uint8       data[SHA_BLOCKSIZE]; /* SHA data buffer */
	int            local;               /* unprocessed amount in data */

	/* tranform function specified by initalization */
	void (*transform_fn)(struct sha1_state_t *sha_info);

} ASSHA1State;

/*****************************************************************************/

typedef struct sha1_state_t SHA_INFO;

/* special Ares version with different constants and init vectors. */
void as_sha1_ares_init (SHA_INFO *);
void as_sha1_init (SHA_INFO *);
void as_sha1_update (SHA_INFO *, const void *, unsigned int);
void as_sha1_final (SHA_INFO *, unsigned char [20]);

/*****************************************************************************/

#if 1
#define GET_BE32(p,ind) \
	((p)[ind] << 24 | (p)[(ind)+1] << 16 | (p)[(ind)+2] << 8 | (p)[(ind)+3])
#else
#define GET_BE32(p32,ind) \
	ntohl((p32)[(ind)/4])
#endif

#define COPY_BE32x16(W,wind,p,ind)             \
    (W)[(wind)]   = GET_BE32((p),(ind));       \
    (W)[(wind)+1] = GET_BE32((p),(ind)+4);     \
    (W)[(wind)+2] = GET_BE32((p),(ind)+8);     \
    (W)[(wind)+3] = GET_BE32((p),(ind)+12);

/* SHA f()-functions */

#define f1(x,y,z)		((x & y) | (~x & z))
#define f2(x,y,z)		(x ^ y ^ z)
#define f3(x,y,z)		((x & y) | (x & z) | (y & z))
#define f4(x,y,z)		(x ^ y ^ z)

/* truncate to 32 bits -- should be a null op on 32-bit machines */

#define T32(x)	((x) & 0xffffffffL)

/* 32-bit rotate */

#define R32(x,n)		T32(((x << n) | (x >> (32 - n))))

/*****************************************************************************/

/* SHA transformation with standard constants */

#define CONST1			0x5a827999L
#define CONST2			0x6ed9eba1L
#define CONST3			0x8f1bbcdcL
#define CONST4			0xca62c1d6L

/* the generic case, for when the overall rotation is not unraveled */

#define FG(n)	\
	T = T32(R32(A,5) + f##n(B,C,D) + E + *WP++ + CONST##n);		\
	E = D; D = C; C = R32(B,30); B = A; A = T

/* specific cases, for when the overall rotation is unraveled */

#define FA(n)	\
	T = T32(R32(A,5) + f##n(B,C,D) + E + *WP++ + CONST##n); B = R32(B,30)

#define FB(n)	\
	E = T32(R32(T,5) + f##n(A,B,C) + D + *WP++ + CONST##n); A = R32(A,30)

#define FC(n)	\
	D = T32(R32(E,5) + f##n(T,A,B) + C + *WP++ + CONST##n); T = R32(T,30)

#define FD(n)	\
	C = T32(R32(D,5) + f##n(E,T,A) + B + *WP++ + CONST##n); E = R32(E,30)

#define FE(n)	\
	B = T32(R32(C,5) + f##n(D,E,T) + A + *WP++ + CONST##n); D = R32(D,30)

#define FT(n)	\
	A = T32(R32(B,5) + f##n(C,D,E) + T + *WP++ + CONST##n); C = R32(C,30)


/* do SHA transformation */
static void sha_transform(SHA_INFO *sha_info)
{
	int i;
	as_uint8 *dp;
	unsigned long T, A, B, C, D, E, W[80], *WP;

	dp = sha_info->data;

	COPY_BE32x16(W, 0,dp,0);
	COPY_BE32x16(W, 4,dp,16);
	COPY_BE32x16(W, 8,dp,32);
	COPY_BE32x16(W,12,dp,48);

	for (i = 16; i < 80; ++i) {
		W[i] = W[i-3] ^ W[i-8] ^ W[i-14] ^ W[i-16];
		W[i] = R32(W[i], 1);
	}

	A = sha_info->digest[0];
	B = sha_info->digest[1];
	C = sha_info->digest[2];
	D = sha_info->digest[3];
	E = sha_info->digest[4];
	WP = W;

	FA(1); FB(1); FC(1); FD(1); FE(1); FT(1); FA(1); FB(1); FC(1); FD(1);
	FE(1); FT(1); FA(1); FB(1); FC(1); FD(1); FE(1); FT(1); FA(1); FB(1);
	FC(2); FD(2); FE(2); FT(2); FA(2); FB(2); FC(2); FD(2); FE(2); FT(2);
	FA(2); FB(2); FC(2); FD(2); FE(2); FT(2); FA(2); FB(2); FC(2); FD(2);
	FE(3); FT(3); FA(3); FB(3); FC(3); FD(3); FE(3); FT(3); FA(3); FB(3);
	FC(3); FD(3); FE(3); FT(3); FA(3); FB(3); FC(3); FD(3); FE(3); FT(3);
	FA(4); FB(4); FC(4); FD(4); FE(4); FT(4); FA(4); FB(4); FC(4); FD(4);
	FE(4); FT(4); FA(4); FB(4); FC(4); FD(4); FE(4); FT(4); FA(4); FB(4);

	sha_info->digest[0] = T32(sha_info->digest[0] + E);
	sha_info->digest[1] = T32(sha_info->digest[1] + T);
	sha_info->digest[2] = T32(sha_info->digest[2] + A);
	sha_info->digest[3] = T32(sha_info->digest[3] + B);
	sha_info->digest[4] = T32(sha_info->digest[4] + C);
}

#undef CONST1
#undef CONST2
#undef CONST3
#undef CONST4
#undef FG
#undef FA
#undef FB
#undef FC
#undef FD
#undef FE
#undef FT

/*****************************************************************************/

/* initialize the SHA digest */
void as_sha1_init(SHA_INFO *sha_info)
{
	/* standard sha1 vectors */
	sha_info->digest[0] = 0x67452301L;
	sha_info->digest[1] = 0xefcdab89L;
	sha_info->digest[2] = 0x98badcfeL;
	sha_info->digest[3] = 0x10325476L;
	sha_info->digest[4] = 0xc3d2e1f0L;

	/* standard sha1 transform function */
	sha_info->transform_fn = sha_transform;

	sha_info->count_lo = 0L;
	sha_info->count_hi = 0L;
	sha_info->local = 0;
}

/* update the SHA digest */
void as_sha1_update(SHA_INFO *sha_info, const void *data, unsigned int count)
{
	int i;
	unsigned long clo;
	const as_uint8 *buffer = data;

	clo = T32(sha_info->count_lo + ((unsigned long) count << 3));
	if (clo < sha_info->count_lo) {
		++sha_info->count_hi;
	}
	sha_info->count_lo = clo;
	sha_info->count_hi += (unsigned long) count >> 29;
	if (sha_info->local) {
		i = SHA_BLOCKSIZE - sha_info->local;
		if (i > count) {
			i = count;
		}
		memcpy(sha_info->data + sha_info->local, buffer, i);
		count -= i;
		buffer += i;
		sha_info->local += i;
		if (sha_info->local == SHA_BLOCKSIZE) {
			sha_info->transform_fn (sha_info);
		} else {
			return;
		}
	}
	while (count >= SHA_BLOCKSIZE) {
		memcpy(sha_info->data, buffer, SHA_BLOCKSIZE);
		buffer += SHA_BLOCKSIZE;
		count -= SHA_BLOCKSIZE;
		sha_info->transform_fn (sha_info);
	}
	memcpy(sha_info->data, buffer, count);
	sha_info->local = count;
}

/* finish computing the SHA digest */
void as_sha1_final(SHA_INFO *sha_info, unsigned char *digest)
{
	int count;
	unsigned long lo_bit_count, hi_bit_count;

	lo_bit_count = sha_info->count_lo;
	hi_bit_count = sha_info->count_hi;
	count = (int) ((lo_bit_count >> 3) & 0x3f);
	sha_info->data[count++] = 0x80;
	if (count > SHA_BLOCKSIZE - 8) {
		memset(sha_info->data + count, 0, SHA_BLOCKSIZE - count);
		sha_info->transform_fn (sha_info);
		memset(sha_info->data, 0, SHA_BLOCKSIZE - 8);
	} else {
		memset(sha_info->data + count, 0,
			SHA_BLOCKSIZE - 8 - count);
	}
	sha_info->data[56] = (unsigned char) ((hi_bit_count >> 24) & 0xff);
	sha_info->data[57] = (unsigned char) ((hi_bit_count >> 16) & 0xff);
	sha_info->data[58] = (unsigned char) ((hi_bit_count >>	8) & 0xff);
	sha_info->data[59] = (unsigned char) ((hi_bit_count >>	0) & 0xff);
	sha_info->data[60] = (unsigned char) ((lo_bit_count >> 24) & 0xff);
	sha_info->data[61] = (unsigned char) ((lo_bit_count >> 16) & 0xff);
	sha_info->data[62] = (unsigned char) ((lo_bit_count >>	8) & 0xff);
	sha_info->data[63] = (unsigned char) ((lo_bit_count >>	0) & 0xff);
	sha_info->transform_fn(sha_info);
	digest[ 0] = (unsigned char) ((sha_info->digest[0] >> 24) & 0xff);
	digest[ 1] = (unsigned char) ((sha_info->digest[0] >> 16) & 0xff);
	digest[ 2] = (unsigned char) ((sha_info->digest[0] >>  8) & 0xff);
	digest[ 3] = (unsigned char) ((sha_info->digest[0]		) & 0xff);
	digest[ 4] = (unsigned char) ((sha_info->digest[1] >> 24) & 0xff);
	digest[ 5] = (unsigned char) ((sha_info->digest[1] >> 16) & 0xff);
	digest[ 6] = (unsigned char) ((sha_info->digest[1] >>  8) & 0xff);
	digest[ 7] = (unsigned char) ((sha_info->digest[1]		) & 0xff);
	digest[ 8] = (unsigned char) ((sha_info->digest[2] >> 24) & 0xff);
	digest[ 9] = (unsigned char) ((sha_info->digest[2] >> 16) & 0xff);
	digest[10] = (unsigned char) ((sha_info->digest[2] >>  8) & 0xff);
	digest[11] = (unsigned char) ((sha_info->digest[2]		) & 0xff);
	digest[12] = (unsigned char) ((sha_info->digest[3] >> 24) & 0xff);
	digest[13] = (unsigned char) ((sha_info->digest[3] >> 16) & 0xff);
	digest[14] = (unsigned char) ((sha_info->digest[3] >>  8) & 0xff);
	digest[15] = (unsigned char) ((sha_info->digest[3]		) & 0xff);
	digest[16] = (unsigned char) ((sha_info->digest[4] >> 24) & 0xff);
	digest[17] = (unsigned char) ((sha_info->digest[4] >> 16) & 0xff);
	digest[18] = (unsigned char) ((sha_info->digest[4] >>  8) & 0xff);
	digest[19] = (unsigned char) ((sha_info->digest[4]		) & 0xff);
}

/*****************************************************************************/

void print_bin_data(unsigned char * data, int len)
{
        int i;
        int i2;
        int i2_end;

		fprintf(stderr, "\ndata len %d\n", len);

        for (i2 = 0; i2 < len; i2 = i2 + 16)
        {
                i2_end = (i2 + 16 > len) ? len: i2 + 16;
                for (i = i2; i < i2_end; i++)
                        if (isprint(data[i]))
                                fprintf(stderr, "%c", data[i]);
                        else
                        fprintf(stderr, ".");
                for ( i = i2_end ; i < i2 + 16; i++)
                        fprintf(stderr, " ");
                fprintf(stderr, " | ");
                for (i = i2; i < i2_end; i++)
                        fprintf(stderr, "%02x ", data[i]);
                fprintf(stderr, "\n");
        }
}

void print_hex_str (unsigned char * data, int len)
{
	int i;

	for(i = 0; i < len; i++)
		printf ("%02x", data[i]);
	printf("\n");
}

/*****************************************************************************/

static const char hex_string[] = "0123456789ABCDEFabcdef";

/* caller frees returned string */
char *as_hex_encode (const unsigned char *data, int src_len)
{
	char *out, *dst;
	int i;

	if (!data)
		return NULL;

	if (! (out = dst = malloc (src_len * 2 + 1)))
		return NULL;

	for(i=0; i<src_len; i++, dst += 2)
	{
		dst[0] = hex_string[data[i] >> 4];
		dst[1] = hex_string[data[i] & 0x0F];
	}

	dst[0] = 0;

	return out;
}

/* caller frees returned string */
unsigned char *as_hex_decode (const char *data, int *dst_len)
{
	char *dst, *h;
	int i, j;

	if (!data)
		return NULL;

	if (! (dst = malloc (strlen (data) / 2 + 1)))
		return NULL;

	for(i=0; *data && data[1]; i++, data += 2)
	{
		unsigned char byte = 0;

		for (j=0; j<2; j++)
		{
			if ((h = strchr (hex_string, data[j])) == NULL)
			{
				free (dst);
				return NULL;
			}

			byte <<= 4;
			byte |= (h - hex_string > 0x0F) ? (h - hex_string - 6) : h - hex_string;
		}

		dst[i] = byte;
	}

	if (dst_len)
		*dst_len = i;

	return dst;
}

/*****************************************************************************/

/* Entry points to the boring part of the new nonce calculation. */
void __fastcall sub_5F0FF4 (void *buf);
void __fastcall sub_5EFB14 (void *buf);
void __fastcall sub_5ECD08 (void *buf);
void __fastcall sub_5F1D64 (void *buf);
void __fastcall sub_5EEAD0 (void *buf);
void __fastcall sub_5F01D8 (void *buf);
void __fastcall sub_5F251C (void *buf);
void __fastcall sub_5F0C40 (void *buf);
void __fastcall sub_5F2F54 (void *buf);
void __fastcall sub_5ED4C0 (void *buf);
void __fastcall sub_5F05C4 (void *buf);
void __fastcall sub_5F3744 (void *buf);
void __fastcall sub_5F45FC (void *buf);
void __fastcall sub_5EC6A0 (void *buf);
void __fastcall sub_5F1878 (void *buf);
void __fastcall sub_5EDE08 (void *buf);
void __fastcall sub_5EE220 (void *buf);
void __fastcall sub_5EF3AC (void *buf);
void __fastcall sub_5F3E8C (void *buf);
void __fastcall sub_5F2828 (void *buf);


/* Calculate 20 byte nonce used in handshake with Ares 2962 and later.
 * Requires supernode GUID from 0x38 packet. Caller frees returned memory.
 */
as_uint8 *as_cipher_nonce2 (as_uint8 guid[16])
{
	/*
	 * Pseudo code of what this does:
	 * 
	 * input: string guid
	 * vars: byte a = 128, byte b = 128, string nonce
	 *
	 * nonce = SHA1 (guid)
	 * while (length (nonce) < 512)
	 * {
	 *     nonce = nonce + SHA1 (byte_string (a) + nonce + byte_string (b));
	 *     a++;
	 *     b--;
	 * }
	 * truncate (nonce, 512)
	 * apply_boring_munging (nonce)
	 * nonce = SHA1 (nonce)
	 *	
	 */
	as_uint8 a = 128, b = 128;
	as_uint8 buf[512+20], *nonce;
	as_uint32 buf_len = 0;
	ASSHA1State sha1_state;

	/* Calc SHA1 of GUID. */
	as_sha1_init (&sha1_state);
	as_sha1_update (&sha1_state, guid, 16);
	as_sha1_final (&sha1_state, buf);
	buf_len = 20;

	/* Make 512 byte SHA1 concat */
	while (buf_len < 512)
	{
		/* Calc SHA1 (a + buf + b) and append to buffer. */
		as_sha1_init (&sha1_state);
		as_sha1_update (&sha1_state, &a, 1);
		as_sha1_update (&sha1_state, buf, buf_len);
		as_sha1_update (&sha1_state, &b, 1);
/*
		assert (buf_len + 20 <= sizeof (buf));
*/		
		as_sha1_final (&sha1_state, buf + buf_len);
		buf_len += 20;
		a++;
		b--;
	}

	/* Truncate buffer. */
	buf_len = 512;

	/* Munge buffer with boring code. */
#define INVOKE(func,buf) __asm lea eax, buf __asm call func

	INVOKE(sub_5F0FF4, buf);
	INVOKE(sub_5EFB14, buf);
	INVOKE(sub_5ECD08, buf);
	INVOKE(sub_5F1D64, buf);
	INVOKE(sub_5EEAD0, buf);
	INVOKE(sub_5F01D8, buf);
	INVOKE(sub_5F251C, buf);
	INVOKE(sub_5F0C40, buf);
	INVOKE(sub_5F2F54, buf);
	INVOKE(sub_5ED4C0, buf);
	INVOKE(sub_5F05C4, buf);
	INVOKE(sub_5F3744, buf);
	INVOKE(sub_5F45FC, buf);
	INVOKE(sub_5EC6A0, buf);
	INVOKE(sub_5F1878, buf);
	INVOKE(sub_5EDE08, buf);
	INVOKE(sub_5EE220, buf);
	INVOKE(sub_5EF3AC, buf);
	INVOKE(sub_5F3E8C, buf);
	INVOKE(sub_5F2828, buf);

	/* Create nonce from buffer. */
	if (!(nonce = malloc (sizeof (as_uint8) * 20)))
		return NULL;

	as_sha1_init (&sha1_state);
	as_sha1_update (&sha1_state, buf, buf_len);
	as_sha1_final (&sha1_state, nonce);

	return nonce;
}

/*****************************************************************************/

int main (int argc, char* argv[])
{
	unsigned char *guid, *nonce;
	int len;

	if (argc != 2)
	{
		fprintf (stderr, "Usage: %s <supernode_guid in hex>\n", argv[0]);
		exit (1);
	}

	if (!(guid = as_hex_decode (argv[1], &len)))
		FATAL_ERROR ("hex decode failed");

	if (len != 16)
		FATAL_ERROR ("guid not 16 bytes in length");

	fprintf (stderr, "guid:   ");
	print_hex_str (guid, len);

	if (!(nonce = as_cipher_nonce2 (guid)))
		FATAL_ERROR ("nonce creation failed");

	fprintf (stderr, "nonce2: ");
	print_hex_str (nonce, 20);
	
	free (nonce);	
	free (guid);
}

/*****************************************************************************/
