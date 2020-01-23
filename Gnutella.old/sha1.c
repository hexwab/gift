/*
 * $Id: sha1.c,v 1.11 2003/07/08 15:11:09 jasta Exp $
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
 * This code is in the public domain.
 */

#include <string.h>
#include <stdio.h>

#include "gt_gnutella.h"

#include "sha1.h"

/*****************************************************************************/

#define SHA_BLOCKSIZE		64
#define SHA_DIGESTSIZE		20

typedef struct {
	unsigned long  digest[5];           /* message digest */
	unsigned long  count_lo, count_hi;  /* 64-bit bit count */
	uint8_t        data[SHA_BLOCKSIZE]; /* SHA data buffer */
	int            local;               /* unprocessed amount in data */
} SHA_INFO;

#ifndef WIN32

/* sigh */
#ifdef WORDS_BIGENDIAN
#  if SIZEOF_LONG == 4
#    define SHA_BYTE_ORDER  4321
#  elif SIZEOF_LONG == 8
#    define SHA_BYTE_ORDER  87654321
#  endif
#else
#  if SIZEOF_LONG == 4
#    define SHA_BYTE_ORDER  1234
#  elif SIZEOF_LONG == 8
#    define SHA_BYTE_ORDER  12345678
#  endif
#endif

#else /* WIN32 */

#define SHA_BYTE_ORDER 1234

#endif /* !WIN32 */

/*****************************************************************************/

void sha_init(SHA_INFO *);
void sha_update(SHA_INFO *, uint8_t *, int);
void sha_final(unsigned char [20], SHA_INFO *);

void sha_stream(unsigned char [20], SHA_INFO *, FILE *);
void sha_print(unsigned char [20]);
char *sha_version(void);

/*****************************************************************************/

#define SHA_VERSION 1

/* UNRAVEL should be fastest & biggest */
/* UNROLL_LOOPS should be just as big, but slightly slower */
/* both undefined should be smallest and slowest */

#define UNRAVEL
/* #define UNROLL_LOOPS */

/* SHA f()-functions */

#define f1(x,y,z)		((x & y) | (~x & z))
#define f2(x,y,z)		(x ^ y ^ z)
#define f3(x,y,z)		((x & y) | (x & z) | (y & z))
#define f4(x,y,z)		(x ^ y ^ z)

/* SHA constants */

#define CONST1			0x5a827999L
#define CONST2			0x6ed9eba1L
#define CONST3			0x8f1bbcdcL
#define CONST4			0xca62c1d6L

/* truncate to 32 bits -- should be a null op on 32-bit machines */

#define T32(x)	((x) & 0xffffffffL)

/* 32-bit rotate */

#define R32(x,n)		T32(((x << n) | (x >> (32 - n))))

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
	uint8_t *dp;
	unsigned long T, A, B, C, D, E, W[80], *WP;

	dp = sha_info->data;

/*
the following makes sure that at least one code block below is
traversed or an error is reported, without the necessity for nested
preprocessor if/else/endif blocks, which are a great pain in the
nether regions of the anatomy...
*/
#undef SWAP_DONE

#if (SHA_BYTE_ORDER == 1234)
#define SWAP_DONE
	for (i = 0; i < 16; ++i) {
		T = *((unsigned long *) dp);
		dp += 4;
		W[i] =	((T << 24) & 0xff000000) | ((T <<  8) & 0x00ff0000) |
				((T >>	8) & 0x0000ff00) | ((T >> 24) & 0x000000ff);
	}
#endif /* SHA_BYTE_ORDER == 1234 */

#if (SHA_BYTE_ORDER == 4321)
#define SWAP_DONE
	for (i = 0; i < 16; ++i) {
		T = *((unsigned long *) dp);
		dp += 4;
		W[i] = T32(T);
	}
#endif /* SHA_BYTE_ORDER == 4321 */

#if (SHA_BYTE_ORDER == 12345678)
#define SWAP_DONE
	for (i = 0; i < 16; i += 2) {
		T = *((unsigned long *) dp);
		dp += 8;
		W[i] =	((T << 24) & 0xff000000) | ((T <<  8) & 0x00ff0000) |
				((T >>	8) & 0x0000ff00) | ((T >> 24) & 0x000000ff);
		T >>= 32;
		W[i+1] = ((T << 24) & 0xff000000) | ((T <<	8) & 0x00ff0000) |
				 ((T >>  8) & 0x0000ff00) | ((T >> 24) & 0x000000ff);
	}
#endif /* SHA_BYTE_ORDER == 12345678 */

#if (SHA_BYTE_ORDER == 87654321)
#define SWAP_DONE
	for (i = 0; i < 16; i += 2) {
		T = *((unsigned long *) dp);
		dp += 8;
		W[i] = T32(T >> 32);
		W[i+1] = T32(T);
	}
#endif /* SHA_BYTE_ORDER == 87654321 */

#ifndef SWAP_DONE
#error Unknown byte order -- you need to add code here
#endif /* SWAP_DONE */

	for (i = 16; i < 80; ++i) {
		W[i] = W[i-3] ^ W[i-8] ^ W[i-14] ^ W[i-16];
#if (SHA_VERSION == 1)
		W[i] = R32(W[i], 1);
#endif /* SHA_VERSION */
	}
	A = sha_info->digest[0];
	B = sha_info->digest[1];
	C = sha_info->digest[2];
	D = sha_info->digest[3];
	E = sha_info->digest[4];
	WP = W;
#ifdef UNRAVEL
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
#else /* !UNRAVEL */
#ifdef UNROLL_LOOPS
	FG(1); FG(1); FG(1); FG(1); FG(1); FG(1); FG(1); FG(1); FG(1); FG(1);
	FG(1); FG(1); FG(1); FG(1); FG(1); FG(1); FG(1); FG(1); FG(1); FG(1);
	FG(2); FG(2); FG(2); FG(2); FG(2); FG(2); FG(2); FG(2); FG(2); FG(2);
	FG(2); FG(2); FG(2); FG(2); FG(2); FG(2); FG(2); FG(2); FG(2); FG(2);
	FG(3); FG(3); FG(3); FG(3); FG(3); FG(3); FG(3); FG(3); FG(3); FG(3);
	FG(3); FG(3); FG(3); FG(3); FG(3); FG(3); FG(3); FG(3); FG(3); FG(3);
	FG(4); FG(4); FG(4); FG(4); FG(4); FG(4); FG(4); FG(4); FG(4); FG(4);
	FG(4); FG(4); FG(4); FG(4); FG(4); FG(4); FG(4); FG(4); FG(4); FG(4);
#else /* !UNROLL_LOOPS */
	for (i =  0; i < 20; ++i) { FG(1); }
	for (i = 20; i < 40; ++i) { FG(2); }
	for (i = 40; i < 60; ++i) { FG(3); }
	for (i = 60; i < 80; ++i) { FG(4); }
#endif /* !UNROLL_LOOPS */
	sha_info->digest[0] = T32(sha_info->digest[0] + A);
	sha_info->digest[1] = T32(sha_info->digest[1] + B);
	sha_info->digest[2] = T32(sha_info->digest[2] + C);
	sha_info->digest[3] = T32(sha_info->digest[3] + D);
	sha_info->digest[4] = T32(sha_info->digest[4] + E);
#endif /* !UNRAVEL */
}

/* initialize the SHA digest */

void sha_init(SHA_INFO *sha_info)
{
	sha_info->digest[0] = 0x67452301L;
	sha_info->digest[1] = 0xefcdab89L;
	sha_info->digest[2] = 0x98badcfeL;
	sha_info->digest[3] = 0x10325476L;
	sha_info->digest[4] = 0xc3d2e1f0L;
	sha_info->count_lo = 0L;
	sha_info->count_hi = 0L;
	sha_info->local = 0;
}

/* update the SHA digest */

void sha_update(SHA_INFO *sha_info, uint8_t *buffer, int count)
{
	int i;
	unsigned long clo;

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
		memcpy(((uint8_t *) sha_info->data) + sha_info->local, buffer, i);
		count -= i;
		buffer += i;
		sha_info->local += i;
		if (sha_info->local == SHA_BLOCKSIZE) {
			sha_transform(sha_info);
		} else {
			return;
		}
	}
	while (count >= SHA_BLOCKSIZE) {
		memcpy(sha_info->data, buffer, SHA_BLOCKSIZE);
		buffer += SHA_BLOCKSIZE;
		count -= SHA_BLOCKSIZE;
		sha_transform(sha_info);
	}
	memcpy(sha_info->data, buffer, count);
	sha_info->local = count;
}

/* finish computing the SHA digest */

void sha_final(unsigned char digest[20], SHA_INFO *sha_info)
{
	int count;
	unsigned long lo_bit_count, hi_bit_count;

	lo_bit_count = sha_info->count_lo;
	hi_bit_count = sha_info->count_hi;
	count = (int) ((lo_bit_count >> 3) & 0x3f);
	((uint8_t *) sha_info->data)[count++] = 0x80;
	if (count > SHA_BLOCKSIZE - 8) {
		memset(((uint8_t *) sha_info->data) + count, 0, SHA_BLOCKSIZE - count);
		sha_transform(sha_info);
		memset((uint8_t *) sha_info->data, 0, SHA_BLOCKSIZE - 8);
	} else {
		memset(((uint8_t *) sha_info->data) + count, 0,
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
	sha_transform(sha_info);
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

/* compute the SHA digest of a FILE stream */

#define BLOCK_SIZE		8192

void sha_stream(unsigned char digest[20], SHA_INFO *sha_info, FILE *fin)
{
	int i;
	uint8_t data[BLOCK_SIZE];

	sha_init(sha_info);
	while ((i = fread(data, 1, BLOCK_SIZE, fin)) > 0) {
		sha_update(sha_info, data, i);
	}
	sha_final(digest, sha_info);
}

/* print a SHA digest */
void sha_print(unsigned char digest[20])
{
	int i, j;

	for (j = 0; j < 5; ++j) {
		for (i = 0; i < 4; ++i) {
			printf("%02x", *digest++);
		}
		printf("%c", (j < 4) ? ' ' : '\n');
	}
}

char *sha_version(void)
{
#if (SHA_VERSION == 1)
	static char *version = "SHA-1";
#else
	static char *version = "SHA";
#endif
	return(version);
}

/*****************************************************************************/

/* Hash a file with the sha1 algorithm using fread.
 * Hash the whole file if size == 0. */
static unsigned char *sha1_hash_fread (const char *file, off_t size)
{
	FILE          *f;
	unsigned char *hash;
	SHA_INFO       state;
	off_t          len;
	ssize_t        n;
	struct stat    st;
	char           buf[BLOCK_SIZE];

	if (!(f = fopen (file, "r")))
		return NULL;

	sha_init (&state);

	if (stat (file, &st) == -1)
	{
		fclose (f);
		return NULL;
	}

	if (size == 0)
		size = st.st_size;

	while (size > 0)
	{
		len = MIN (sizeof (buf), size);

		n = fread (buf, 1, len, f);

		if (n == 0 || n != len)
			break;

		sha_update (&state, (unsigned char *) buf, len);
		size -= len;
	}

	fclose (f);

	if (size != 0)
		return NULL;

	if ((hash = malloc (SHA1_BINSIZE)))
		sha_final (hash, &state);

	return hash;
}

/*****************************************************************************/

// Convert 5 Bytes to 8 Bytes Base32
static void _Sha1toBase32(char *out, const uint8_t *in)
{
	const char *Table = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

	out[0] = Table[((in[0] >> 3)               ) & 0x1F];
	out[1] = Table[((in[0] << 2) | (in[1] >> 6)) & 0x1F];
	out[2] = Table[((in[1] >> 1)               ) & 0x1F];
	out[3] = Table[((in[1] << 4) | (in[2] >> 4)) & 0x1F];
	out[4] = Table[((in[2] << 1) | (in[3] >> 7)) & 0x1F];
	out[5] = Table[((in[3] >> 2)               ) & 0x1F];
	out[6] = Table[((in[3] << 3) | (in[4] >> 5)) & 0x1F];
	out[7] = Table[((in[4]     )               ) & 0x1F];
}

// Return a base32 representation of a sha1 hash
char *sha1_string (unsigned char *sha1)
{
	char *str;
	char *base32;

	str = malloc (SHA1_STRLEN + 1);

	if (!str)
		return NULL;

	base32 = str;

	_Sha1toBase32 ((uint8_t *)base32, sha1);
	_Sha1toBase32 ((uint8_t *)base32 + 8, sha1 + 5);
	_Sha1toBase32 ((uint8_t *)base32 + 16, sha1 + 10);
	_Sha1toBase32 ((uint8_t *)base32 + 24, sha1 + 15);

	str[SHA1_STRLEN] = 0;

	return str;
}

/*****************************************************************************/

/* Convert 8 bytes Base32 to 5 bytes */
static void _Base32toBin (char *alphabet, const uint8_t *bits, uint8_t *out,
                          const char *base32)
{
	const unsigned char *in = base32;

	out[0] = ((bits[in[0]]       ) << 3) | (bits[in[1]] & 0x1C) >> 2;
	out[1] = ((bits[in[1]] & 0x03) << 6) | (bits[in[2]]       ) << 1
	                                     | (bits[in[3]] & 0x10) >> 4;
	out[2] = ((bits[in[3]] & 0x0F) << 4) | (bits[in[4]] & 0x1E) >> 1;
	out[3] = ((bits[in[4]] & 0x01) << 7) | (bits[in[5]]       ) << 2
	                                     | (bits[in[6]] & 0x18) >> 3;
	out[4] = ((bits[in[6]] & 0x07) << 5) | (bits[in[7]]);
}

unsigned char *sha1_bin (char *ascii)
{
	char          *alphabet  = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
	static uint8_t bits[256] = { 0 };
	unsigned char *bin;
	int            i;

	/* goto produces better code on gcc 2.9x for this */
	if (bits['b'] == 0)
		goto init_table;

convert_str:
	if (!(bin = malloc (SHA1_BINSIZE)))
		return NULL;

	assert (strlen (ascii) == 32);

	_Base32toBin (alphabet, bits, bin     , ascii     );
	_Base32toBin (alphabet, bits, bin + 5 , ascii +  8);
	_Base32toBin (alphabet, bits, bin + 10, ascii + 16);
	_Base32toBin (alphabet, bits, bin + 15, ascii + 24);

	return bin;

init_table:
	/* set the each char's corresponding bit value in a lookup table */
	for (i = 0; i < sizeof (bits); i++)
	{
		char *pos;

		if ((pos = strchr (alphabet, toupper (i))))
			bits[i] = pos - alphabet;
	}
	goto convert_str;
}

/*****************************************************************************/

unsigned char *sha1_digest (const char *file, off_t size)
{
	unsigned char *hash;

	if (!file)
		return NULL;

	hash = sha1_hash_fread (file, size);

	return hash;
}

unsigned char *sha1_dup (unsigned char *sha1)
{
	unsigned char *new_sha1;

	if (!(new_sha1 = malloc (SHA1_BINSIZE)))
		return NULL;

	memcpy (new_sha1, sha1, SHA1_BINSIZE);

	return new_sha1;
}

/*****************************************************************************/

#if 0
void test_str (char *test)
{
	unsigned char *bin;
	char          *hash;

	assert (bin = sha1_bin (test));

	hash = sha1_string (bin);

	if (strcmp (hash+5, test) != 0)
		fprintf (stderr, "test=%s\nhash=%s\n", test, hash);

	free (bin);
	free (hash);
}

int main(int argc, char **argv)
{
	int   i, j;
	char *alpha = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
	char  str[33];
	int   len   = strlen (alpha);

	str[sizeof (str) - 1] = 0;

	test_str (alpha);

	for (i = 1; i <= 100000; i++)
	{
		for (j = 0; j < 32; j++)
			str[j] = alpha[rand () % len];

		fprintf (stderr, "\r%i", i);
		test_str (str);
	}

	fprintf(stderr, "\n");
	return 0;
}

int main(int argc, char **argv)
{
	int i;

	for (i = 1; i < argc; i++)
	{
		unsigned char *bin;
		char          *str0;

		if (!(bin = sha1_digest (argv[i], 0)))
		{
			perror("");
			continue;
		}

		if ((str0 = sha1_string (bin)))
		{
			char *str;

			str = str0;
			string_sep (&str, ":");

			printf ("%s\t%s\n", basename (argv[i]), str);

			free (str0);
			free (bin);
		}

	}

	return 0;
}
#endif

#if 0
int main(int argc, char **argv)
{
	int i;
	FILE *f;
	SHA_INFO sha_info;
	char *str;

	for (i = 1; i < argc; i++)
	{
		unsigned char *digest;

		if (!(digest = malloc (sizeof (char) * 20)))
			continue;

		if (!(f = fopen (argv[i], "r")))
		{
			free (digest);
			continue;
		}

		sha_stream (digest, &sha_info, f);
		str = sha1_string (digest);

		printf("%s\t%s\n", str, argv[i]);

		free (digest);
		free (str);
		fclose (f);
	}
	return 0;
}
#endif

