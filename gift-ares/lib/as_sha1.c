/*
 * $Id: as_sha1.c,v 1.2 2004/09/02 19:39:52 mkern Exp $
 *
 * (PD) 2001 The Bitzi Corporation
 * Please see http://bitzi.com/publicdomain for more info.
 *
 * NIST Secure Hash Algorithm
 * heavily modified by Uwe Hollerbach <uh@alumni.caltech edu>
 * from Peter C. Gutmann's implementation as found in
 * Applied Cryptography by Bruce Schneier
 * Further modifications to include the "UNRAVEL" stuff, below
 *
 * New, faster sha1 code. The original code was from Bitzi corporation, and
 * was in the public domain. [The original header is included.]
 *
 * Interface adapted for Ares library. Added as_sha1_ares_init which uses
 * different initalization vectors and constants.
 *
 * This code is in the public domain.
 */

#include "as_ares.h"

/*****************************************************************************/

typedef struct sha1_state_t SHA_INFO;

void as_sha1_init (SHA_INFO *);
void as_sha1_update (SHA_INFO *, const void *, unsigned int);
void as_sha1_final (SHA_INFO *, unsigned char [20]);

/* special Ares version with different constants and init vectors. */
void as_sha1_ares_init (SHA_INFO *);

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

/* SHA transformation with special Ares constants */

#define CONST1			0x5a827901L
#define CONST2			0x6ed9eba1L
#define CONST3			0x1f1cbcdcL
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
static void sha_ares_transform(SHA_INFO *sha_info)
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

/* special Ares version with different constants and init vectors. */
void as_sha1_ares_init (SHA_INFO *sha_info)
{
	/* ares vectors */
	sha_info->digest[0] = 0x17452301L;
	sha_info->digest[1] = 0xeacdaf89L;
	sha_info->digest[2] = 0x98bfdcfeL;
	sha_info->digest[3] = 0x14325476L;
	sha_info->digest[4] = 0xc3d2c1f0L;

	/* Ares transform function with different constants */
	sha_info->transform_fn = sha_ares_transform;

	sha_info->count_lo = 0L;
	sha_info->count_hi = 0L;
	sha_info->local = 0;
}

/*****************************************************************************/

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

/*****************************************************************************/

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

