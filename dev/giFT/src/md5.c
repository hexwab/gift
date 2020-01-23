/*
 * md5.c
 *
 * This code implements the MD5 message-digest algorithm.
 * The algorithm is due to Ron Rivest.  This code was
 * written by Colin Plumb in 1993, no copyright is claimed.
 * This code is in the public domain; do with it what you wish.
 *
 * Equivalent code is available from RSA Data Security, Inc.
 * This code has been tested against that, and is equivalent,
 * except that you don't need to include two pages of legalese
 * with every copy.
 *
 * To compute the message digest of a chunk of bytes, declare an
 * MD5Context structure, pass it to MD5Init, call MD5Update as
 * needed on buffers full of bytes, and then call MD5Final, which
 * will fill a supplied 16-byte array with the digest.
 */

#include "gift.h"

#include "md5.h"
#include "file.h"

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#ifndef WIN32
# include <sys/stat.h>
# include <sys/types.h>
# ifdef HAVE_SYS_MMAN_H
#  include <sys/mman.h>
#  ifndef MAP_FAILED
#   define MAP_FAILED ((void *) -1)
#  endif
# endif
#else
# include <io.h> /* open() */
# define open(p1, p2) _open(p1, p2)
# define read(p1, p2, p3) _read(p1, p2, p3)
# define close(p1) _close(p1)
#endif /* !WIN32 */

#ifdef WORDS_BIGENDIAN
# define HIGHFIRST
#endif /* WORDS_BIGENDIAN */

/*****************************************************************************/

#ifdef HAVE_MMAP
#define USE_MMAP
#endif

/* mmap is messing with people */
#undef USE_MMAP

#define HASH_LEN     16

/*****************************************************************************/

struct MD5Context
{
	ft_uint32     buf[4];
	ft_uint32     bits[2];
	unsigned char in[64];
};

static void MD5Init(struct MD5Context *context);
static void MD5Update(struct MD5Context *context, unsigned char const *buf,
                      unsigned len);
static void MD5Final(unsigned char digest[HASH_LEN], struct MD5Context *context);
static void MD5Transform(ft_uint32 buf[4], ft_uint32 const in[HASH_LEN]);

/*
 * This is needed to make RSAREF happy on some MS-DOS compilers.
 */
typedef struct MD5Context MD5_CTX;
# define md5_init MD5Init
# define md5_state_t MD5_CTX
# define md5_append MD5Update
# define md5_finish MD5Final

/*****************************************************************************/

#ifndef HIGHFIRST
# define byteReverse(buf, len)
/* Nothing */
#else /* HIGHFIRST */
static void byteReverse(unsigned char *buf, unsigned longs);

# ifndef ASM_MD5
/*
 * Note: this code is harmless on little-endian machines.
 */
static void byteReverse(unsigned char *buf, unsigned longs)
{
    ft_uint32 t;
    do
	{
		t = (ft_uint32) ((unsigned) buf[3] << 8 | buf[2]) << 16 |
			((unsigned) buf[1] << 8 | buf[0]);
		*(ft_uint32 *) buf = t;
		buf += 4;
    }
	while (--longs);
}
# endif /* !ASM_MD5 */
#endif /* !HIGHFIRST */

/*
 * Start MD5 accumulation.  Set bit count to 0 and buffer to mysterious
 * initialization constants.
 */
static void MD5Init(struct MD5Context *ctx)
{
    ctx->buf[0] = 0x67452301;
    ctx->buf[1] = 0xefcdab89;
    ctx->buf[2] = 0x98badcfe;
    ctx->buf[3] = 0x10325476;

    ctx->bits[0] = 0;
    ctx->bits[1] = 0;
}

/*
 * Update context to reflect the concatenation of another buffer full
 * of bytes.
 */
static void MD5Update(struct MD5Context *ctx, unsigned char const *buf, unsigned len)
{
    ft_uint32 t;

    /* Update bitcount */

    t = ctx->bits[0];
    if ((ctx->bits[0] = t + ((ft_uint32) len << 3)) < t)
		ctx->bits[1]++;

	/* Carry from low to high */
    ctx->bits[1] += len >> 29;

    t = (t >> 3) & 0x3f;
	/* Bytes already in shsInfo->data */

    /* Handle any leading odd-sized chunks */
    if (t)
	{
		unsigned char *p = (unsigned char *) ctx->in + t;
		t = 64 - t;
		if (len < t)
		{
			memcpy(p, buf, len);
			return;
		}

		memcpy(p, buf, t);
		byteReverse(ctx->in, HASH_LEN);
		MD5Transform(ctx->buf, (ft_uint32 *) ctx->in);
		buf += t;
		len -= t;
    }
    /* Process data in 64-byte chunks */

    while (len >= 64)
	{
#ifdef HIGHFIRST
		memcpy(ctx->in, buf, 64);
		byteReverse(ctx->in, HASH_LEN);
		MD5Transform(ctx->buf, (ft_uint32 *) ctx->in);
#else
		/* not using memcpy() gives us 5% speed gain on little-endian machines */
		MD5Transform(ctx->buf, (ft_uint32 *) buf);
#endif /* HIGHFIRST */
		buf += 64;
		len -= 64;
    }

    /* Handle any remaining bytes of data. */
    memcpy(ctx->in, buf, len);
}

/*
 * Final wrapup - pad to 64-byte boundary with the bit pattern
 * 1 0* (64-bit count of bits processed, MSB-first)
 */
static void MD5Final(unsigned char digest[HASH_LEN], struct MD5Context *ctx)
{
    unsigned count;
    unsigned char *p;

    /* Compute number of bytes mod 64 */
    count = (ctx->bits[0] >> 3) & 0x3F;

    /* Set the first char of padding to 0x80.  This is safe since there is
	 always at least one byte free */
    p = ctx->in + count;
    *p++ = 0x80;

    /* Bytes of padding needed to make 64 bytes */
    count = 64 - 1 - count;

    /* Pad out to 56 mod 64 */
    if (count < 8)
	{
		/* Two lots of padding:  Pad the first block to 64 bytes */
		memset(p, 0, count);
		byteReverse(ctx->in, HASH_LEN);
		MD5Transform(ctx->buf, (ft_uint32 *) ctx->in);

		/* Now fill the next block with 56 bytes */
		memset(ctx->in, 0, 56);
    }
	else
	{
		/* Pad block to 56 bytes */
		memset(p, 0, count - 8);
    }
    byteReverse(ctx->in, 14);

    /* Append length in bits and transform */
    ((ft_uint32 *) ctx->in)[14] = ctx->bits[0];
    ((ft_uint32 *) ctx->in)[15] = ctx->bits[1];

    MD5Transform(ctx->buf, (ft_uint32 *) ctx->in);
    byteReverse((unsigned char *) ctx->buf, 4);
    memcpy(digest, ctx->buf, HASH_LEN);
    memset(ctx, 0, sizeof(ctx));
	/* In case it's sensitive */
}

#ifndef ASM_MD5

/* The four core functions - F1 is optimized somewhat */

/* #define F1(x, y, z) (x & y | ~x & z) */
# define F1(x, y, z) (z ^ (x & (y ^ z)))
# define F2(x, y, z) F1(z, x, y)
# define F3(x, y, z) (x ^ y ^ z)
# define F4(x, y, z) (y ^ (x | ~z))

/* This is the central step in the MD5 algorithm. */
# define MD5STEP(f, w, x, y, z, data, s) ( w += f(x, y, z) + data,  w = (w<<s) | (w>>(32-s)),  w += x )

/*
 * The core of the MD5 algorithm, this alters an existing MD5 hash to
 * reflect the addition of 16 longwords of new data.  MD5Update blocks
 * the data and converts bytes into longwords for this routine.
 */
static void MD5Transform(ft_uint32 buf[4], ft_uint32 const in[HASH_LEN])
{
    register ft_uint32 a, b, c, d;

    a = buf[0];
    b = buf[1];
    c = buf[2];
    d = buf[3];

    MD5STEP(F1, a, b, c, d, in[0] + 0xd76aa478, 7);
    MD5STEP(F1, d, a, b, c, in[1] + 0xe8c7b756, 12);
    MD5STEP(F1, c, d, a, b, in[2] + 0x242070db, 17);
    MD5STEP(F1, b, c, d, a, in[3] + 0xc1bdceee, 22);
    MD5STEP(F1, a, b, c, d, in[4] + 0xf57c0faf, 7);
    MD5STEP(F1, d, a, b, c, in[5] + 0x4787c62a, 12);
    MD5STEP(F1, c, d, a, b, in[6] + 0xa8304613, 17);
    MD5STEP(F1, b, c, d, a, in[7] + 0xfd469501, 22);
    MD5STEP(F1, a, b, c, d, in[8] + 0x698098d8, 7);
    MD5STEP(F1, d, a, b, c, in[9] + 0x8b44f7af, 12);
    MD5STEP(F1, c, d, a, b, in[10] + 0xffff5bb1, 17);
    MD5STEP(F1, b, c, d, a, in[11] + 0x895cd7be, 22);
    MD5STEP(F1, a, b, c, d, in[12] + 0x6b901122, 7);
    MD5STEP(F1, d, a, b, c, in[13] + 0xfd987193, 12);
    MD5STEP(F1, c, d, a, b, in[14] + 0xa679438e, 17);
    MD5STEP(F1, b, c, d, a, in[15] + 0x49b40821, 22);

    MD5STEP(F2, a, b, c, d, in[1] + 0xf61e2562, 5);
    MD5STEP(F2, d, a, b, c, in[6] + 0xc040b340, 9);
    MD5STEP(F2, c, d, a, b, in[11] + 0x265e5a51, 14);
    MD5STEP(F2, b, c, d, a, in[0] + 0xe9b6c7aa, 20);
    MD5STEP(F2, a, b, c, d, in[5] + 0xd62f105d, 5);
    MD5STEP(F2, d, a, b, c, in[10] + 0x02441453, 9);
    MD5STEP(F2, c, d, a, b, in[15] + 0xd8a1e681, 14);
    MD5STEP(F2, b, c, d, a, in[4] + 0xe7d3fbc8, 20);
    MD5STEP(F2, a, b, c, d, in[9] + 0x21e1cde6, 5);
    MD5STEP(F2, d, a, b, c, in[14] + 0xc33707d6, 9);
    MD5STEP(F2, c, d, a, b, in[3] + 0xf4d50d87, 14);
    MD5STEP(F2, b, c, d, a, in[8] + 0x455a14ed, 20);
    MD5STEP(F2, a, b, c, d, in[13] + 0xa9e3e905, 5);
    MD5STEP(F2, d, a, b, c, in[2] + 0xfcefa3f8, 9);
    MD5STEP(F2, c, d, a, b, in[7] + 0x676f02d9, 14);
    MD5STEP(F2, b, c, d, a, in[12] + 0x8d2a4c8a, 20);

    MD5STEP(F3, a, b, c, d, in[5] + 0xfffa3942, 4);
    MD5STEP(F3, d, a, b, c, in[8] + 0x8771f681, 11);
    MD5STEP(F3, c, d, a, b, in[11] + 0x6d9d6122, 16);
    MD5STEP(F3, b, c, d, a, in[14] + 0xfde5380c, 23);
    MD5STEP(F3, a, b, c, d, in[1] + 0xa4beea44, 4);
    MD5STEP(F3, d, a, b, c, in[4] + 0x4bdecfa9, 11);
    MD5STEP(F3, c, d, a, b, in[7] + 0xf6bb4b60, 16);
    MD5STEP(F3, b, c, d, a, in[10] + 0xbebfbc70, 23);
    MD5STEP(F3, a, b, c, d, in[13] + 0x289b7ec6, 4);
    MD5STEP(F3, d, a, b, c, in[0] + 0xeaa127fa, 11);
    MD5STEP(F3, c, d, a, b, in[3] + 0xd4ef3085, 16);
    MD5STEP(F3, b, c, d, a, in[6] + 0x04881d05, 23);
    MD5STEP(F3, a, b, c, d, in[9] + 0xd9d4d039, 4);
    MD5STEP(F3, d, a, b, c, in[12] + 0xe6db99e5, 11);
    MD5STEP(F3, c, d, a, b, in[15] + 0x1fa27cf8, 16);
    MD5STEP(F3, b, c, d, a, in[2] + 0xc4ac5665, 23);

    MD5STEP(F4, a, b, c, d, in[0] + 0xf4292244, 6);
    MD5STEP(F4, d, a, b, c, in[7] + 0x432aff97, 10);
    MD5STEP(F4, c, d, a, b, in[14] + 0xab9423a7, 15);
    MD5STEP(F4, b, c, d, a, in[5] + 0xfc93a039, 21);
    MD5STEP(F4, a, b, c, d, in[12] + 0x655b59c3, 6);
    MD5STEP(F4, d, a, b, c, in[3] + 0x8f0ccc92, 10);
    MD5STEP(F4, c, d, a, b, in[10] + 0xffeff47d, 15);
    MD5STEP(F4, b, c, d, a, in[1] + 0x85845dd1, 21);
    MD5STEP(F4, a, b, c, d, in[8] + 0x6fa87e4f, 6);
    MD5STEP(F4, d, a, b, c, in[15] + 0xfe2ce6e0, 10);
    MD5STEP(F4, c, d, a, b, in[6] + 0xa3014314, 15);
    MD5STEP(F4, b, c, d, a, in[13] + 0x4e0811a1, 21);
    MD5STEP(F4, a, b, c, d, in[4] + 0xf7537e82, 6);
    MD5STEP(F4, d, a, b, c, in[11] + 0xbd3af235, 10);
    MD5STEP(F4, c, d, a, b, in[2] + 0x2ad7d2bb, 15);
    MD5STEP(F4, b, c, d, a, in[9] + 0xeb86d391, 21);

    buf[0] += a;
    buf[1] += b;
    buf[2] += c;
    buf[3] += d;
}

#endif /* !ASM_MD5 */

/*****************************************************************************/
/* the following code was modified by the giFT project */

/* hashes a file using ordinary read() */
static unsigned char *md5_hash_file (char *file, off_t size)
{
	md5_state_t   state;
	char         *hash;
	char         *buf;
	ssize_t       buf_n;
	int           fd;
    struct stat   st;
	off_t         st_size;
	unsigned long st_blksize;

	if (!file)
		return NULL;

	/* TODO -- does windows have an fstat? */
	if (stat (file, &st) < 0)
	{
		GIFT_ERROR (("Can't stat %s: %s", file, GIFT_STRERROR ()));
		return NULL;
	}

	/* we need to open the file and get the size */
	if ((fd = open (file, O_RDONLY)) < 0)
	{
		GIFT_ERROR (("Can't open %s: %s", file, GIFT_STRERROR ()));
		return NULL;
	}

	/* take in the stat suggestions */
	st_size    = st.st_size;
#ifndef WIN32
    st_blksize = st.st_blksize;
#else /* WIN32 */
    st_blksize = 1024; /* get 1024 byte per turn */
#endif /* !WIN32 */

	/* if size is set appropriately use it */
	if (size && size < st_size)
		st_size = size;

	if (!(buf = malloc (st_blksize)))
		return NULL;

	/* initialize the md5 state */
	md5_init (&state);

	while ((buf_n = read (fd, buf, MIN ((off_t) st_blksize, st_size))) > 0)
	{
		md5_append (&state, (unsigned char *) buf, buf_n);
		st_size -= buf_n;

		if (st_size <= 0)
			break;
	}

	/* TODO: do we have to md5_deinit or something? */
	if ((hash = malloc (HASH_LEN + 1)))
		md5_finish (hash, &state);

	/* cleanup */
	free (buf);
	close (fd);

	return hash;
}

/*****************************************************************************/

#if 0
/* hashes a file using mmap */
static unsigned char *md5_hash_file_mmap(char *file, unsigned long size,
                                         unsigned char** digest)
{
	md5_state_t   state;
	char         *m;
	size_t        file_size;
# ifndef WIN32
	int           fd;
	struct stat   st;
	char          buf[1];
# else /* WIN32 */
	HANDLE        file_h;
	HANDLE        mapping_h;
# endif /* !WIN32 */

	assert (file);
	assert (digest);
	if (!file || !digest)
		return NULL;

	/* we need to open the file and get the size */
# ifndef WIN32
	if ((fd = open (file, O_RDONLY)) < 0)
	{
		GIFT_ERROR (("Can't open %s: %s", file, GIFT_STRERROR ()));
		return NULL;
	}
	if (fstat (fd, &st) < 0)
	{
		GIFT_ERROR (("Can't stat %s: %s", file, GIFT_STRERROR ()));
		close (fd);
		return NULL;
	}
	file_size = st.st_size;
	/* we need to read at least 1 byte, to see if the file is readable
	 * otherwise, mmap segfaults when it tries to read it */
	if ((read (fd, buf, 1) != 1) || (lseek (fd, 0, SEEK_SET) == -1))
	{
		GIFT_ERROR (("Can't read %s: %s", file, GIFT_STRERROR ()));
		return NULL;
	}
# else /* WIN32 */
	file_h = CreateFile (file, GENERIC_READ, FILE_SHARE_READ,
	                     NULL, OPEN_EXISTING, 0, NULL);

	if (file_h == INVALID_HANDLE_VALUE)
	{
		GIFT_ERROR (("CreateFile: %s", GIFT_STRERROR ()));
		return NULL;
	}

	if ((file_size = GetFileSize (file_h, NULL)) == 0xFFFFFFFF)
	{
		GIFT_ERROR (("GetFileSize: %s", GIFT_STRERROR ()));
		CloseHandle (file_h);
		return NULL;
	}
# endif /* !WIN32 */

	if (!size || size > file_size)
		size = file_size;

	/* mmap the file */
# ifndef WIN32
	if ((m = mmap (NULL, size, PROT_READ, MAP_PRIVATE, fd, 0)) == MAP_FAILED)
	{
		GIFT_ERROR (("mmap failed on %s: %s", file, GIFT_STRERROR ()));
		return NULL;
	}

#  ifdef HAVE_MADVISE
	madvise (m, size, MADV_SEQUENTIAL);
#  endif /* HAVE_MADVISE */

# else /* WIN32 */
	if ((mapping_h = CreateFileMapping (file_h, NULL, PAGE_READONLY,
	                                    0, 0, NULL)) == NULL)
	{
		GIFT_ERROR (("CreateFileMapping: %s", GIFT_STRERROR ()));
		CloseHandle (file_h);
		return NULL;
	}

	if ((m = MapViewOfFile (mapping_h, FILE_MAP_READ, 0, 0,
	                        size)) == NULL)
	{
		GIFT_ERROR (("MapViewOfFile: %s", GIFT_STRERROR ()));
		CloseHandle (mapping_h);
		CloseHandle (file_h);
		return NULL;
	}
# endif /* !WIN32 */

	/* actually calculate the md5 */
	md5_init (&state);
	md5_append (&state, (unsigned char *) m, size);
	md5_finish (*digest, &state);

	/* cleanup */
# ifndef WIN32
	munmap (m, size);
	close (fd);
# else /* WIN32 */
	UnmapViewOfFile (m);
	CloseHandle (mapping_h);
	CloseHandle (file_h);
# endif /* !WIN32 */

	return *digest;
}
#endif

/*****************************************************************************/

unsigned char *md5_digest (char *file, off_t size)
{
	unsigned char *hash;

	if (!file)
		return NULL;

#ifdef USE_MMAP
	if ((hash = md5_hash_file_mmap (file, size)))
		return hash;

	GIFT_WARN (("mmap failed for %s, falling back to manual read..."));
#endif /* USE_MMAP */

	hash = md5_hash_file (file, size);

	return hash;
}

unsigned char *md5_dup (unsigned char *md5)
{
	unsigned char *dup;

	if (!(dup = malloc (HASH_LEN)))
		return NULL;

	memcpy (dup, md5, HASH_LEN);
	return dup;
}

/*****************************************************************************/

static int bin_to_hex (unsigned char *bin, char *hex, int len)
{
	static char table[16] =
	{
		'0', '1', '2', '3', '4', '5', '6', '7', '8', '9',
		'a', 'b', 'c', 'd', 'e', 'f'
	};

	if (!bin || !hex)
		return FALSE;

	while (len-- > 0)
	{
		unsigned char c = *bin++;

		*hex++ = table[(c & 0xf0) >> 4];
		*hex++ = table[(c & 0x0f)];
	}

	*hex++ = 0;

	return TRUE;
}

/* returns the md5 sum as an ASCII string */
char *md5_string (unsigned char *md5)
{
	char *md5_ascii;

	if (!md5)
		return NULL;

	if (!(md5_ascii = malloc (HASH_LEN * 2 + 1)))
		return NULL;

	if (!bin_to_hex (md5, md5_ascii, HASH_LEN))
	{
		free (md5_ascii);
		return NULL;
	}

	return md5_ascii;
}

static unsigned char hex_char_to_bin (char x)
{
	if (x >= '0' && x <= '9')
		return (x - '0');

	x = toupper (x);

	return ((x - 'A') + 10);
}

static int hex_to_bin (char *hex, unsigned char *bin, int len)
{
	unsigned char value;

	while (isxdigit (hex[0]) && isxdigit (hex[1]) && len-- > 0)
	{
		value  = (hex_char_to_bin (*hex++) << 4) & 0xf0;
		value |= (hex_char_to_bin (*hex++)       & 0x0f);
		*bin++ = value;
	}

   return (len <= 0) ? TRUE : FALSE;
}

unsigned char *md5_bin (char *md5_ascii)
{
	unsigned char *md5;

	if (!md5_ascii)
		return NULL;

	if (!(md5 = malloc (HASH_LEN)))
		return NULL;

	if (!hex_to_bin (md5_ascii, md5, HASH_LEN))
	{
		free (md5);
		return NULL;
	}

	return md5;
}

/*****************************************************************************/

char *md5_checksum (char *file, off_t size)
{
	char *md5;
	char *md5_ascii;

	if (!(md5 = md5_digest (file, size)))
		return NULL;

	md5_ascii = md5_string (md5);
	free (md5);

	return md5_ascii;
}

/*****************************************************************************/

#if 0
/*
 * gcc -o md5 md5.c [lib_objects] -g -Wall -DHAVE_CONFIG_H -I../lib -I..
 *
 * replace [lib_objects] with *.o in ../lib/.  If you think hard about it
 * you'll know why I can't actually type that out ;)
 */
int main (int argc, char **argv)
{
	char *sum;

	if (!(sum = md5_checksum (argv[1], 0)))
		return 1;

	printf ("%s\n", sum);
	printf ("%s\n", md5_string (md5_bin (sum)));

	free (sum);

	return 0;
}
#endif
