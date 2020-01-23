/*
 * $Id: fst_hash.c,v 1.15 2004/11/10 20:00:57 mkern Exp $
 *
 * Copyright (C) 2003 giFT-FastTrack project
 * http://developer.berlios.de/projects/gift-fasttrack
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

#ifndef HASH_TEST
# include "fst_fasttrack.h"
#else
# include <stdlib.h>
# include <stdio.h>
# include <string.h>
# include <assert.h>
# include <sys/stat.h>
# include <sys/time.h>
#endif

#include "fst_hash.h"
#include "fst_utils.h"
#include "md5.h"

/*****************************************************************************/

#define FST_HASH_BLOCK_SIZE 4096 /* read block size for file hashing */

#ifndef MIN
# define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

/*****************************************************************************/

/* see below for data */
static const uint32 smalltable[256];
static const uint16 checksumtable[256];

/*
 * Updates 4 byte small hash that is concatenated to the md5 of the first
 * 307200 bytes of the file. Set hash to 0xFFFFFFFF for first run.
 */
uint32 fst_hash_small (uint32 smallhash, const uint8 *data, size_t len)
{
	size_t i;

	for (i = 0; i < len; i++)
		smallhash = smalltable[data[i] ^ (smallhash & 0xff)] ^ (smallhash >> 8);

	return smallhash;
}

/*****************************************************************************/

#ifndef HASH_TEST

/* HACK: When hashing files giFT will call us for the fthash and for kzhash
 * per file because there is no other way to make it handle both hashes.
 * These two calls occur right after another so we cache the result and if
 * it's the same file use the hash from the cache. Yes, hash and path are not
 * freed before shutdown.
 * To further complicate things this is also called by the main thread for 
 * verifying files. Since this can happen while the hashing thread is using
 * the cache bad things may ensue. When forking this is not a problem, on
 * windows we are using a mutex to protect the cache.
 */
static struct _hcache
{
	FSTHash *hash;
	char *path;

#ifdef WIN32
	HANDLE hMutex;
#endif
}
#ifdef WIN32
hcache = { NULL, NULL, NULL };
#else
hcache = { NULL, NULL };
#endif


static FSTHash *cache_get_hash (const char* path)
{
	FSTHash *hash;

#ifdef WIN32
	/* create mutex if not created, potential race condition here */
	if (!hcache.hMutex)
	{
		FST_HEAVY_DBG ("creating hash cache mutex");

		hcache.hMutex = CreateMutex (NULL, FALSE, NULL);
		if (hcache.hMutex == ERROR_INVALID_HANDLE)
		{
			hcache.hMutex = NULL;
			return NULL; /* oh my */
		}
	}
#endif

#ifdef WIN32
	if (WaitForSingleObject (hcache.hMutex, INFINITE) == WAIT_FAILED)
		return NULL;
#endif

	if (!hcache.path || strcmp (path, hcache.path))
	{
#ifdef WIN32
		ReleaseMutex (hcache.hMutex);
#endif
		return NULL;
	}

	hash = hcache.hash;
	hcache.hash = NULL;
	free (hcache.path);
	hcache.path = NULL;

#ifdef WIN32
	ReleaseMutex (hcache.hMutex);
#endif

	return hash;
}

static BOOL cache_set_hash (const char* path, FSTHash *hash)
{
#ifdef WIN32
	/* create mutex if not created, potential race condition here */
	if (!hcache.hMutex)
	{
		FST_HEAVY_DBG ("creating hash cache mutex");

		hcache.hMutex = CreateMutex (NULL, FALSE, NULL);
		if (hcache.hMutex == ERROR_INVALID_HANDLE)
		{
			hcache.hMutex = NULL;
			return FALSE; /* oh my */
		}
	}
#endif


#ifdef WIN32
	if (WaitForSingleObject (hcache.hMutex, INFINITE) == WAIT_FAILED)
		return FALSE;
#endif

	fst_hash_free (hcache.hash);
	hcache.hash = hash;
	free (hcache.path);
	hcache.path = strdup (path);

#ifdef WIN32
	ReleaseMutex (hcache.hMutex);
#endif

	return TRUE;
}

/* our primary hash is the kzhash... */
unsigned char *fst_giftcb_kzhash (const char *path, size_t *len)
{
	unsigned char *data;
	FSTHash *hash;

	if (!(data = malloc (FST_KZHASH_LEN)))
		return NULL;

	if ((hash = cache_get_hash (path)))
	{
		/* cache hit */
		FST_HEAVY_DBG_1 ("cache hit: %s", path);
		memcpy (data, FST_KZHASH (hash), FST_KZHASH_LEN);

		fst_hash_free (hash);
		*len = FST_KZHASH_LEN;
		return data;
	}

	/* cache miss */
	FST_HEAVY_DBG_1 ("cache miss: %s", path);

	/* calculate the hash */
	if (!(hash = fst_hash_create ()))
	{
		free (data);
		return NULL;
	}

	if (!fst_hash_file (hash, path))
	{
		free (data);
		fst_hash_free (hash);
		return NULL;
	}

	memcpy (data, FST_KZHASH (hash), FST_KZHASH_LEN);

	/* write hash to cache */
	cache_set_hash (path, hash);

	*len = FST_KZHASH_LEN;
	return data;
}

char *fst_giftcb_kzhash_encode (unsigned char *data)
{
	FSTHash *hash;
	char *str;

	if (!(hash = fst_hash_create_raw (data, FST_KZHASH_LEN)))
		return NULL;

	str = strdup (fst_hash_encode16_kzhash (hash));

	fst_hash_free (hash);

	return str;
}

/* ...but we also do download verification using fthash */
unsigned char *fst_giftcb_fthash (const char *path, size_t *len)
{
	unsigned char *data;
	FSTHash *hash;

	if (!(data = malloc (FST_FTHASH_LEN)))
		return NULL;

	if ((hash = cache_get_hash (path)))
	{
		/* cache hit */
		FST_HEAVY_DBG_1 ("cache hit: %s", path);
		memcpy (data, FST_KZHASH (hash), FST_FTHASH_LEN);

		fst_hash_free (hash);
		*len = FST_FTHASH_LEN;
		return data;
	}

	/* cache miss */
	FST_HEAVY_DBG_1 ("cache miss: %s", path);

	/* calculate the hash */
	if (!(hash = fst_hash_create ()))
	{
		free (data);
		return NULL;
	}

	if (!fst_hash_file (hash, path))
	{
		free (data);
		fst_hash_free (hash);
		return NULL;
	}

	memcpy (data, FST_FTHASH (hash), FST_FTHASH_LEN);

	/* write hash to cache */
	cache_set_hash (path, hash);

	*len = FST_FTHASH_LEN;
	return data;

}

char *fst_giftcb_fthash_encode (unsigned char *data)
{
	FSTHash *hash;
	char *str;

	if (!(hash = fst_hash_create_raw (data, FST_FTHASH_LEN)))
		return NULL;

	str = strdup (fst_hash_encode64_fthash (hash));

	fst_hash_free (hash);

	return str;
}

#endif

/*****************************************************************************/

static void hash_clear (FSTHash *hash)
{
	memset (&hash->data, 0, sizeof (hash->data));
	free (hash->ctx);
	hash->ctx = NULL;
}

/* create new hash object */
FSTHash *fst_hash_create ()
{
	FSTHash *hash;

	if (!(hash = malloc (sizeof (FSTHash))))
		return NULL;

	hash->ctx = NULL;
	hash_clear (hash);

	return hash;
}

/* create new hash object from org_hash */
FSTHash *fst_hash_create_copy (FSTHash *org_hash)
{
	FSTHash *hash;

	if (!(hash = fst_hash_create ()))
		return NULL;


	if (!fst_hash_set_raw (hash, FST_KZHASH (org_hash), FST_KZHASH_LEN))
	{
		fst_hash_free (hash);
		return NULL;
	}

	return hash;
}


/* create new hash object from raw data */
FSTHash *fst_hash_create_raw (const uint8 *data, size_t len)
{
	FSTHash *hash;

	if (!(hash = fst_hash_create ()))
		return NULL;

	if (!fst_hash_set_raw (hash, data, len))
	{
		fst_hash_free (hash);
		return NULL;
	}

	return hash;
}

/* free hash object */
void fst_hash_free (FSTHash *hash)
{
	if (!hash)
		return;

	free (hash->ctx);
	free (hash);
}

/* set from raw data */
BOOL fst_hash_set_raw (FSTHash* hash, const uint8 *data, size_t len)
{
	hash_clear (hash);

	if (len != FST_KZHASH_LEN && len != FST_FTHASH_LEN)
		return FALSE;

	memcpy (hash->data, data, len);

	return TRUE;
}

/* returns TRUE if md5tree is present, FALSE otherwise */
BOOL fst_hash_have_md5tree (FSTHash *hash)
{
	uint32 *md5tree;

	if (!hash)
		return FALSE;

	md5tree = (uint32*) FST_MD5TREE (hash);

	return (md5tree[0] || md5tree[1] || md5tree[2] || md5tree[3]);
}

/* returns TRUE if the hashes are equivalent. If one of the hashes doesn't
 * have the md5tree only the fthash is compared.
 */
BOOL fst_hash_equal (FSTHash *a, FSTHash *b)
{
	if (!a || !b)
		return FALSE;

	if (!fst_hash_have_md5tree (a) ||
	    !fst_hash_have_md5tree (b))
	{
		return (memcmp (FST_FTHASH (a), FST_FTHASH (b), FST_FTHASH_LEN) == 0);
	}
	else
	{
		return (memcmp (FST_KZHASH (a), FST_KZHASH (b), FST_KZHASH_LEN) == 0);
	}
}

/* returns checksum of fzhash */
uint16 fst_hash_checksum (FSTHash *hash)
{
	uint16 sum = 0;
	int i;

	/* calculate 2 byte checksum used in the URL from 20 byte fthash */
	for (i = 0; i < FST_FTHASH_LEN; i++)
		sum = checksumtable[FST_FTHASH (hash)[i] ^ (sum >> 8)] ^ (sum << 8);

	return (sum & 0x3fff);
}

/*****************************************************************************/

/* clear hash and prepare for updates */
void fst_hash_init (FSTHash *hash)
{
	hash_clear (hash);

	hash->ctx = malloc (sizeof (FSTHashContext));

	MD5Init (&hash->ctx->md5300_ctx);
	MD5Init (&hash->ctx->md5tree_ctx);
	hash->ctx->smallhash = 0xFFFFFFFF;
	hash->ctx->prev_smallhash = 0xFFFFFFFF;
	hash->ctx->pos = 0;
	hash->ctx->sample_pos = 0x100000; /* 1 MiB */
	hash->ctx->wnd_start = 0;
	hash->ctx->index = 0;
	hash->ctx->blocks = 0;
}

/* update hash */
void fst_hash_update (FSTHash *hash, const uint8 *data, size_t inlen)
{
	FSTHashContext *ctx = hash->ctx;
	size_t len, off;

	/* calculate md5300 */
	if (ctx->pos < FST_FTSEG_SIZE)
	{
		len = MIN (FST_FTSEG_SIZE - ctx->pos, inlen);
		MD5Update (&ctx->md5300_ctx, data, len);
	}

	/* keep rolling window of last FST_FTSEG_SIZE bytes */
	if (inlen >= FST_FTSEG_SIZE)
	{
		memcpy (ctx->wnd, data + inlen - FST_FTSEG_SIZE, FST_FTSEG_SIZE);
		ctx->wnd_start = 0;
	}
	else
	{
		len = MIN (FST_FTSEG_SIZE - ctx->wnd_start, inlen);
		memcpy (ctx->wnd + ctx->wnd_start, data, len);
		memcpy (ctx->wnd, data + len, inlen - len);
		ctx->wnd_start = (ctx->wnd_start + inlen) % FST_FTSEG_SIZE;
	}

	/* calculate smallhash */
	while (ctx->sample_pos < ctx->pos + inlen)
	{
		if (ctx->sample_pos >= ctx->pos)
		{
			/* save smallhash so we can restore it later if necessary */
			ctx->prev_smallhash = ctx->smallhash;
			
			off = ctx->sample_pos - ctx->pos;
			len = MIN (FST_FTSEG_SIZE, inlen - off);
			ctx->smallhash = fst_hash_small (ctx->smallhash, data + off, len);
		}
		else
		{
			len = MIN (FST_FTSEG_SIZE - (ctx->pos - ctx->sample_pos), inlen);
			ctx->smallhash = fst_hash_small (ctx->smallhash, data, len);
		}

		if (ctx->sample_pos + FST_FTSEG_SIZE > ctx->pos + inlen)
			break;

		ctx->sample_pos <<= 1;
	}

	/* calculate md5tree */
	off = 0;
	len = MIN (FST_KZSEG_SIZE - (ctx->pos % FST_KZSEG_SIZE), inlen);

	while (len)
	{
		MD5Update (&ctx->md5tree_ctx, data + off, len);
		off += len;

		if ((ctx->pos + off) % FST_KZSEG_SIZE == 0)
		{
			size_t b;
			MD5Final (ctx->nodes + ctx->index, &ctx->md5tree_ctx);
			ctx->index += MD5_HASH_LEN;
			assert (ctx->index <= sizeof (ctx->nodes));
			ctx->blocks++;

			/* collapse tree */
			for (b = ctx->blocks; !(b & 1); b >>= 1)
			{
				MD5Init (&ctx->md5tree_ctx);
				MD5Update (&ctx->md5tree_ctx, ctx->nodes + ctx->index - 2*MD5_HASH_LEN, 2*MD5_HASH_LEN);
				MD5Final (ctx->nodes + ctx->index - 2*MD5_HASH_LEN,&ctx->md5tree_ctx);
				ctx->index -= MD5_HASH_LEN;
			}

			MD5Init (&ctx->md5tree_ctx);
		}

		/* go on with next block */
		len = MIN (FST_KZSEG_SIZE, inlen - off);
	}

	/* move position and wait for next chunk */
	ctx->pos += inlen;
}

/* finish hashing */
void fst_hash_finish (FSTHash *hash)
{
	FSTHashContext *ctx = hash->ctx;
	size_t len;

	/* finish md5300 */
	MD5Final (FST_MD5300 (hash), &ctx->md5300_ctx);

	/* finish smallhash */
	if (ctx->pos > FST_FTSEG_SIZE)
	{
		if (ctx->sample_pos >= ctx->pos)
			ctx->sample_pos >>= 1;

		if (ctx->sample_pos + FST_FTSEG_SIZE > ctx->pos - FST_FTSEG_SIZE)
		{
			/* last hashed block within FST_FTSEG_SIZE bytes of end, rewind */
			ctx->smallhash = ctx->prev_smallhash;
		}

		/* hash end of file */
		len = MIN (ctx->pos - FST_FTSEG_SIZE, FST_FTSEG_SIZE);
		ctx->wnd_start = (ctx->wnd_start + (FST_FTSEG_SIZE - len)) % FST_FTSEG_SIZE;

		ctx->smallhash = fst_hash_small (ctx->smallhash,
			                             ctx->wnd + ctx->wnd_start,
	                                     MIN (FST_FTSEG_SIZE - ctx->wnd_start, len));

		ctx->smallhash = fst_hash_small (ctx->smallhash,
		                                 ctx->wnd,
	                                     len - MIN (FST_FTSEG_SIZE - ctx->wnd_start, len));
	}

	ctx->smallhash ^= ctx->pos; /* xor smallhash with filesize */

	FST_SMALLHASH (hash) [0] = ctx->smallhash & 0xff;
	FST_SMALLHASH (hash) [1] = (ctx->smallhash >> 8) & 0xff;
	FST_SMALLHASH (hash) [2] = (ctx->smallhash >> 16) & 0xff;
	FST_SMALLHASH (hash) [3] = (ctx->smallhash >> 24) & 0xff;

	/* finish md5tree */
	if (ctx->pos % FST_KZSEG_SIZE)
	{
		size_t b;
		MD5Final (ctx->nodes + ctx->index, &ctx->md5tree_ctx);
		ctx->index += MD5_HASH_LEN;
		assert (ctx->index <= sizeof (ctx->nodes));
		ctx->blocks++;

		/* collapse tree */
	    for (b = ctx->blocks; !(b & 1); b >>= 1)
		{
			MD5Init (&ctx->md5tree_ctx);
			MD5Update (&ctx->md5tree_ctx, ctx->nodes + ctx->index - 2*MD5_HASH_LEN, 2*MD5_HASH_LEN);
			MD5Final (ctx->nodes + ctx->index - 2*MD5_HASH_LEN, &ctx->md5tree_ctx);
			ctx->index -= MD5_HASH_LEN;
		}
	}

	if (ctx->pos == 0)
	{
		/* 0 input */
		MD5Init (&ctx->md5tree_ctx);
		MD5Final (ctx->nodes + ctx->index, &ctx->md5tree_ctx);
	}
	else if (ctx->blocks == 1)
	{
		/* <= FST_KZSEG_SIZE input */
		MD5Init (&ctx->md5tree_ctx);
		MD5Update (&ctx->md5tree_ctx, ctx->nodes + ctx->index - MD5_HASH_LEN, MD5_HASH_LEN);
		MD5Final (ctx->nodes + ctx->index - MD5_HASH_LEN, &ctx->md5tree_ctx);
	}
	else
	{
		while (!(ctx->blocks & 1))
			ctx->blocks >>= 1;
		ctx->blocks &= ~1;

		while (ctx->blocks)
		{
			MD5Init (&ctx->md5tree_ctx);

			if (ctx->blocks & 1)
			{
				MD5Update (&ctx->md5tree_ctx, ctx->nodes + ctx->index - 2*MD5_HASH_LEN, 2*MD5_HASH_LEN);
				MD5Final (ctx->nodes + ctx->index - 2*MD5_HASH_LEN, &ctx->md5tree_ctx);
				ctx->index -= MD5_HASH_LEN;
			}
			else
			{
				MD5Update (&ctx->md5tree_ctx, ctx->nodes + ctx->index - MD5_HASH_LEN, MD5_HASH_LEN);
				MD5Final (ctx->nodes + ctx->index - MD5_HASH_LEN, &ctx->md5tree_ctx);
			}

			ctx->blocks >>= 1;
		}
	}

	memcpy (FST_MD5TREE (hash), ctx->nodes, MD5_HASH_LEN);
	
	free (hash->ctx);
	hash->ctx = NULL;
}


/* hash entire file */
BOOL fst_hash_file (FSTHash *hash, const char *file)
{
	FILE *fp;
	uint8 buf[FST_HASH_BLOCK_SIZE];
	size_t len;

	if (!hash || !file)
		return FALSE;

	fst_hash_init (hash);

	if (!(fp = fopen (file, "rb")))
		return FALSE;

	while ((len = fread (buf, sizeof (uint8), FST_HASH_BLOCK_SIZE, fp)))
	{
		fst_hash_update (hash, buf, len);
	}

	fclose (fp);

	fst_hash_finish (hash);

	return TRUE;
}

/*****************************************************************************/

/* parse hex encoded kzhash */
BOOL fst_hash_decode16_kzhash (FSTHash *hash, const char *kzhash)
{
	uint8 *buf;
	int len;

	hash_clear (hash);
	
	if (!(buf = fst_utils_hex_decode (kzhash, &len)))
		return FALSE;

	if (len < FST_KZHASH_LEN)
	{
		free (buf);
		return FALSE;
	}
	
	memcpy (FST_KZHASH (hash), buf, FST_KZHASH_LEN);
	free (buf);

	return TRUE;
}

/* parse hex encoded fthash */
BOOL fst_hash_decode16_fthash (FSTHash *hash, const char *fthash)
{
	uint8 *buf;
	int len;

	hash_clear (hash);
	
	if (!(buf = fst_utils_hex_decode (fthash, &len)))
		return FALSE;

	if (len < FST_FTHASH_LEN)
	{
		free (buf);
		return FALSE;
	}
	
	memcpy (FST_FTHASH (hash), buf, FST_FTHASH_LEN);
	free (buf);

	return TRUE;
}

/* parse base64 encoded kzhash */
BOOL fst_hash_decode64_kzhash (FSTHash *hash, const char *kzhash)
{
	uint8 *buf;
	int len;

	hash_clear (hash);
	
	if (!(buf = fst_utils_base64_decode (kzhash, &len)))
		return FALSE;

	if (len < FST_KZHASH_LEN)
	{
		free (buf);
		return FALSE;
	}
	
	memcpy (FST_KZHASH (hash), buf, FST_KZHASH_LEN);
	free (buf);

	return TRUE;
}

/* parse base64 encoded fthash */
BOOL fst_hash_decode64_fthash (FSTHash *hash, const char *fthash)
{
	uint8 *buf;
	int len;

	hash_clear (hash);
	
	if (!(buf = fst_utils_base64_decode (fthash, &len)))
		return FALSE;

	if (len < FST_FTHASH_LEN)
	{
		free (buf);
		return FALSE;
	}
	
	memcpy (FST_FTHASH (hash), buf, FST_FTHASH_LEN);
	free (buf);

	return TRUE;
}

/*****************************************************************************/

/* return pointer to static, hex encoded kzhash string */
char *fst_hash_encode16_kzhash (FSTHash *hash)
{
	static char str[128];
	char *buf;

	if (!(buf = fst_utils_hex_encode (FST_KZHASH (hash), FST_KZHASH_LEN)))
		return NULL;

	if (strlen (buf) >= 128)
	{
		free (buf);
		return NULL;
	}

	strcpy (str, buf);
	free (buf);

	return str;	
}

/* return pointer to static, hex encoded fthash string */
char *fst_hash_encode16_fthash (FSTHash *hash)
{
	static char str[128];
	char *buf;

	if (!(buf = fst_utils_hex_encode (FST_FTHASH (hash), FST_FTHASH_LEN)))
		return NULL;

	if (strlen (buf) >= 128)
	{
		free (buf);
		return NULL;
	}

	strcpy (str, buf);
	free (buf);

	return str;	
}

/* return pointer to static, base64 encoded kzhash string */
char *fst_hash_encode64_kzhash (FSTHash *hash)
{
	static char str[128];
	char *buf;

	if (!(buf = fst_utils_base64_encode (FST_KZHASH (hash), FST_KZHASH_LEN)))
		return NULL;

	if (strlen (buf) >= 128)
	{
		free (buf);
		return NULL;
	}

	strcpy (str, buf);
	free (buf);

	return str;	
}

/* return pointer to static, base64 encoded fthash string */
char *fst_hash_encode64_fthash (FSTHash *hash)
{
	static char str[128];
	char *buf;

	if (!(buf = fst_utils_base64_encode (FST_FTHASH (hash), FST_FTHASH_LEN)))
		return NULL;

	if (strlen (buf) > 127)
	{
		free (buf);
		return NULL;
	}

	/* the leading '=' padding is used by kazaa and sig2dat */
	str[0] = '=';
	strcpy (str+1, buf);
	free (buf);

	return str;	
}

/*****************************************************************************/

static const uint32 smalltable[256] =
{
	0x00000000,0x77073096,0xEE0E612C,0x990951BA,
	0x076DC419,0x706AF48F,0xE963A535,0x9E6495A3,
	0x0EDB8832,0x79DCB8A4,0xE0D5E91E,0x97D2D988,
	0x09B64C2B,0x7EB17CBD,0xE7B82D07,0x90BF1D91,
	0x1DB71064,0x6AB020F2,0xF3B97148,0x84BE41DE,
	0x1ADAD47D,0x6DDDE4EB,0xF4D4B551,0x83D385C7,
	0x136C9856,0x646BA8C0,0xFD62F97A,0x8A65C9EC,
	0x14015C4F,0x63066CD9,0xFA0F3D63,0x8D080DF5,
	0x3B6E20C8,0x4C69105E,0xD56041E4,0xA2677172,
	0x3C03E4D1,0x4B04D447,0xD20D85FD,0xA50AB56B,
	0x35B5A8FA,0x42B2986C,0xDBBBC9D6,0xACBCF940,
	0x32D86CE3,0x45DF5C75,0xDCD60DCF,0xABD13D59,
	0x26D930AC,0x51DE003A,0xC8D75180,0xBFD06116,
	0x21B4F4B5,0x56B3C423,0xCFBA9599,0xB8BDA50F,
	0x2802B89E,0x5F058808,0xC60CD9B2,0xB10BE924,
	0x2F6F7C87,0x58684C11,0xC1611DAB,0xB6662D3D,
	0x76DC4190,0x01DB7106,0x98D220BC,0xEFD5102A,
	0x71B18589,0x06B6B51F,0x9FBFE4A5,0xE8B8D433,
	0x7807C9A2,0x0F00F934,0x9609A88E,0xE10E9818,
	0x7F6A0DBB,0x086D3D2D,0x91646C97,0xE6635C01,
	0x6B6B51F4,0x1C6C6162,0x856530D8,0xF262004E,
	0x6C0695ED,0x1B01A57B,0x8208F4C1,0xF50FC457,
	0x65B0D9C6,0x12B7E950,0x8BBEB8EA,0xFCB9887C,
	0x62DD1DDF,0x15DA2D49,0x8CD37CF3,0xFBD44C65,
	0x4DB26158,0x3AB551CE,0xA3BC0074,0xD4BB30E2,
	0x4ADFA541,0x3DD895D7,0xA4D1C46D,0xD3D6F4FB,
	0x4369E96A,0x346ED9FC,0xAD678846,0xDA60B8D0,
	0x44042D73,0x33031DE5,0xAA0A4C5F,0xDD0D7CC9,
	0x5005713C,0x270241AA,0xBE0B1010,0xC90C2086,
	0x5768B525,0x206F85B3,0xB966D409,0xCE61E49F,
	0x5EDEF90E,0x29D9C998,0xB0D09822,0xC7D7A8B4,
	0x59B33D17,0x2EB40D81,0xB7BD5C3B,0xC0BA6CAD,
	0xEDB88320,0x9ABFB3B6,0x03B6E20C,0x74B1D29A,
	0xEAD54739,0x9DD277AF,0x04DB2615,0x73DC1683,
	0xE3630B12,0x94643B84,0x0D6D6A3E,0x7A6A5AA8,
	0xE40ECF0B,0x9309FF9D,0x0A00AE27,0x7D079EB1,
	0xF00F9344,0x8708A3D2,0x1E01F268,0x6906C2FE,
	0xF762575D,0x806567CB,0x196C3671,0x6E6B06E7,
	0xFED41B76,0x89D32BE0,0x10DA7A5A,0x67DD4ACC,
	0xF9B9DF6F,0x8EBEEFF9,0x17B7BE43,0x60B08ED5,
	0xD6D6A3E8,0xA1D1937E,0x38D8C2C4,0x4FDFF252,
	0xD1BB67F1,0xA6BC5767,0x3FB506DD,0x48B2364B,
	0xD80D2BDA,0xAF0A1B4C,0x36034AF6,0x41047A60,
	0xDF60EFC3,0xA867DF55,0x316E8EEF,0x4669BE79,
	0xCB61B38C,0xBC66831A,0x256FD2A0,0x5268E236,
	0xCC0C7795,0xBB0B4703,0x220216B9,0x5505262F,
	0xC5BA3BBE,0xB2BD0B28,0x2BB45A92,0x5CB36A04,
	0xC2D7FFA7,0xB5D0CF31,0x2CD99E8B,0x5BDEAE1D,
	0x9B64C2B0,0xEC63F226,0x756AA39C,0x026D930A,
	0x9C0906A9,0xEB0E363F,0x72076785,0x05005713,
	0x95BF4A82,0xE2B87A14,0x7BB12BAE,0x0CB61B38,
	0x92D28E9B,0xE5D5BE0D,0x7CDCEFB7,0x0BDBDF21,
	0x86D3D2D4,0xF1D4E242,0x68DDB3F8,0x1FDA836E,
	0x81BE16CD,0xF6B9265B,0x6FB077E1,0x18B74777,
	0x88085AE6,0xFF0F6A70,0x66063BCA,0x11010B5C,
	0x8F659EFF,0xF862AE69,0x616BFFD3,0x166CCF45,
	0xA00AE278,0xD70DD2EE,0x4E048354,0x3903B3C2,
	0xA7672661,0xD06016F7,0x4969474D,0x3E6E77DB,
	0xAED16A4A,0xD9D65ADC,0x40DF0B66,0x37D83BF0,
	0xA9BCAE53,0xDEBB9EC5,0x47B2CF7F,0x30B5FFE9,
	0xBDBDF21C,0xCABAC28A,0x53B39330,0x24B4A3A6,
	0xBAD03605,0xCDD70693,0x54DE5729,0x23D967BF,
	0xB3667A2E,0xC4614AB8,0x5D681B02,0x2A6F2B94,
	0xB40BBE37,0xC30C8EA1,0x5A05DF1B,0x2D02EF8D
};

static const uint16 checksumtable[256] =
{
	0x0000,0x1021,0x2042,0x3063,0x4084,0x50A5,0x60C6,0x70E7,
	0x8108,0x9129,0xA14A,0xB16B,0xC18C,0xD1AD,0xE1CE,0xF1EF,
	0x1231,0x0210,0x3273,0x2252,0x52B5,0x4294,0x72F7,0x62D6,
	0x9339,0x8318,0xB37B,0xA35A,0xD3BD,0xC39C,0xF3FF,0xE3DE,
	0x2462,0x3443,0x0420,0x1401,0x64E6,0x74C7,0x44A4,0x5485,
	0xA56A,0xB54B,0x8528,0x9509,0xE5EE,0xF5CF,0xC5AC,0xD58D,
	0x3653,0x2672,0x1611,0x0630,0x76D7,0x66F6,0x5695,0x46B4,
	0xB75B,0xA77A,0x9719,0x8738,0xF7DF,0xE7FE,0xD79D,0xC7BC,
	0x48C4,0x58E5,0x6886,0x78A7,0x0840,0x1861,0x2802,0x3823,
	0xC9CC,0xD9ED,0xE98E,0xF9AF,0x8948,0x9969,0xA90A,0xB92B,
	0x5AF5,0x4AD4,0x7AB7,0x6A96,0x1A71,0x0A50,0x3A33,0x2A12,
	0xDBFD,0xCBDC,0xFBBF,0xEB9E,0x9B79,0x8B58,0xBB3B,0xAB1A,
	0x6CA6,0x7C87,0x4CE4,0x5CC5,0x2C22,0x3C03,0x0C60,0x1C41,
	0xEDAE,0xFD8F,0xCDEC,0xDDCD,0xAD2A,0xBD0B,0x8D68,0x9D49,
	0x7E97,0x6EB6,0x5ED5,0x4EF4,0x3E13,0x2E32,0x1E51,0x0E70,
	0xFF9F,0xEFBE,0xDFDD,0xCFFC,0xBF1B,0xAF3A,0x9F59,0x8F78,
	0x9188,0x81A9,0xB1CA,0xA1EB,0xD10C,0xC12D,0xF14E,0xE16F,
	0x1080,0x00A1,0x30C2,0x20E3,0x5004,0x4025,0x7046,0x6067,
	0x83B9,0x9398,0xA3FB,0xB3DA,0xC33D,0xD31C,0xE37F,0xF35E,
	0x02B1,0x1290,0x22F3,0x32D2,0x4235,0x5214,0x6277,0x7256,
	0xB5EA,0xA5CB,0x95A8,0x8589,0xF56E,0xE54F,0xD52C,0xC50D,
	0x34E2,0x24C3,0x14A0,0x0481,0x7466,0x6447,0x5424,0x4405,
	0xA7DB,0xB7FA,0x8799,0x97B8,0xE75F,0xF77E,0xC71D,0xD73C,
	0x26D3,0x36F2,0x0691,0x16B0,0x6657,0x7676,0x4615,0x5634,
	0xD94C,0xC96D,0xF90E,0xE92F,0x99C8,0x89E9,0xB98A,0xA9AB,
	0x5844,0x4865,0x7806,0x6827,0x18C0,0x08E1,0x3882,0x28A3,
	0xCB7D,0xDB5C,0xEB3F,0xFB1E,0x8BF9,0x9BD8,0xABBB,0xBB9A,
	0x4A75,0x5A54,0x6A37,0x7A16,0x0AF1,0x1AD0,0x2AB3,0x3A92,
	0xFD2E,0xED0F,0xDD6C,0xCD4D,0xBDAA,0xAD8B,0x9DE8,0x8DC9,
	0x7C26,0x6C07,0x5C64,0x4C45,0x3CA2,0x2C83,0x1CE0,0x0CC1,
	0xEF1F,0xFF3E,0xCF5D,0xDF7C,0xAF9B,0xBFBA,0x8FD9,0x9FF8,
	0x6E17,0x7E36,0x4E55,0x5E74,0x2E93,0x3EB2,0x0ED1,0x1EF0
};

/*****************************************************************************/

#ifdef HASH_TEST

#define INT_MAX 0xFFFFFFFF

/* rndlcg  Linear Congruential Method, the "minimal standard generator"
 *
 * Park & Miller, 1988, Comm of the ACM, 31(10), pp. 1192-1201
 */
static int quotient  = INT_MAX / 16807L;
static int remain = INT_MAX % 16807L;
static int seed_val = 1L;

/* returns a random unsigned integer */
static unsigned int randlcg()
{
        if ( seed_val <= quotient )
                seed_val = (seed_val * 16807L) % INT_MAX;
        else
        {
                int high_part = seed_val / quotient;
                int low_part  = seed_val % quotient;

                int test = 16807L * low_part - remain * high_part;

                if ( test > 0 )
                        seed_val = test;
                else
                        seed_val = test + INT_MAX;

        }

        return seed_val;
}

/* r250.c  the r250 uniform random number algorithm
 *
 * Kirkpatrick, S., and E. Stoll, 1981; "A Very Fast
 * Shift-Register Sequence Random Number Generator",
 * Journal of Computational Physics, V.40
 * also:
 * see W.L. Maier, DDJ May 1991
 */
#define BITS 32
#define MSB          0x80000000L
#define ALL_BITS     0xffffffffL
#define HALF_RANGE   0x40000000L
#define STEP         7

static unsigned int r250_buffer[ 250 ];
static int r250_index;

static void r250_init(int sd)
{
	int j, k;
	unsigned int mask, msb;

	seed_val = sd;
	
	r250_index = 0;
	for (j = 0; j < 250; j++)      /* fill r250 buffer with BITS-1 bit values */
		r250_buffer[j] = randlcg();


	for (j = 0; j < 250; j++)	/* set some MSBs to 1 */
		if ( randlcg() > HALF_RANGE )
			r250_buffer[j] |= MSB;


	msb = MSB;	        /* turn on diagonal bit */
	mask = ALL_BITS;	/* turn off the leftmost bits */

	for (j = 0; j < BITS; j++)
	{
		k = STEP * j + 3;	/* select a word to operate on */
		r250_buffer[k] &= mask; /* turn off bits left of the diagonal */
		r250_buffer[k] |= msb;	/* turn on the diagonal bit */
		mask >>= 1;
		msb  >>= 1;
	}

}

/* returns a random unsigned integer */
static unsigned int r250()
{
	register int	j;
	register unsigned int new_rand;

	if ( r250_index >= 147 )
		j = r250_index - 147;	/* wrap pointer around */
	else
		j = r250_index + 103;

	new_rand = r250_buffer[ r250_index ] ^ r250_buffer[ j ];
	r250_buffer[ r250_index ] = new_rand;

	if ( r250_index >= 249 )	/* increment pointer for next time */
		r250_index = 0;
	else
		r250_index++;

	return new_rand;

}

static const struct
{
	size_t len;
	uint8 kzhash[FST_KZHASH_LEN];
}
test_vectors[] = 
{
	{ 0,                        "\xd4\x1d\x8c\xd9\x8f\x00\xb2\x04\xe9\x80\x09\x98\xec\xf8\x42\x7e\xff\xff\xff\xff\xd4\x1d\x8c\xd9\x8f\x00\xb2\x04\xe9\x80\x09\x98\xec\xf8\x42\x7e" },
	{ 1024,                     "\x73\x25\x7c\xe1\x6d\x52\xaf\xab\x44\xad\x87\xc4\x85\x78\x2e\x03\xff\xfb\xff\xff\x79\x4a\x0e\x41\x18\xe2\x8a\x60\x7e\x42\xc3\x2a\xfa\xf5\xc4\x93" },
	{ 1024 * 32,                "\x42\x10\x84\x84\xbd\xda\xfd\x35\xb1\xa3\x6b\x0f\xd3\xcc\x3f\xa6\xff\x7f\xff\xff\x13\x4f\x29\x36\x3b\x2a\x33\x98\xc5\x7c\x9a\x1c\x5e\x7a\xad\xa2" },
	{ 1024 * 64,                "\x16\x81\xfc\x47\x97\x28\xa5\xda\x1d\x47\x28\xaa\x2d\x24\x83\x61\xff\xff\xfe\xff\xf4\x00\x5e\x16\xcb\x06\xb3\xbe\x56\xac\x51\xe1\x6e\xf3\xa4\xd2" },
	{ 1024 * 300,               "\x0d\x24\xc8\xcb\x0f\xfa\x3f\x55\xfd\xfa\xc1\x75\xc9\x2d\xfe\xf6\xff\x4f\xfb\xff\xfe\x6e\x02\x96\x37\x83\x9a\xd1\x6f\x55\xc4\xda\x1a\x94\x2b\x95" },
	{ 1024 * 500,               "\x0d\x24\xc8\xcb\x0f\xfa\x3f\x55\xfd\xfa\xc1\x75\xc9\x2d\xfe\xf6\x2c\x02\x5e\xd8\xb4\x10\x12\x15\x41\x1e\x50\x36\x25\xea\xcd\x61\x72\xb3\x2a\x89" },
	{ 1024 * 600,               "\x0d\x24\xc8\xcb\x0f\xfa\x3f\x55\xfd\xfa\xc1\x75\xc9\x2d\xfe\xf6\xee\x3f\x60\xd0\x03\xa7\x59\x73\xb8\xe1\xbe\xb0\x4e\xf8\xbb\xce\x8b\x5a\x9d\x8c" },
	{ 1024 * 700,               "\x0d\x24\xc8\xcb\x0f\xfa\x3f\x55\xfd\xfa\xc1\x75\xc9\x2d\xfe\xf6\xad\x2e\xa3\xee\x65\x8b\x38\x57\xab\x27\xfb\xcd\x85\x40\xae\xe6\xdc\xda\x4e\xe4" },
	{ 1024 * 1024,              "\x0d\x24\xc8\xcb\x0f\xfa\x3f\x55\xfd\xfa\xc1\x75\xc9\x2d\xfe\xf6\x64\x05\x3b\x5b\x4d\xb8\x6c\x17\xfb\x2c\x6b\x61\x13\xf6\xec\x7b\x3d\xd8\x81\x1b" },
	{ 1024 * 1536,              "\x0d\x24\xc8\xcb\x0f\xfa\x3f\x55\xfd\xfa\xc1\x75\xc9\x2d\xfe\xf6\xe4\x04\xcb\x0c\xe1\x84\x85\x9e\x46\x50\x06\x65\x59\xcb\xcb\x36\x6d\x84\x92\x9b" },
	{ 1024 * 1024 * 3,          "\x0d\x24\xc8\xcb\x0f\xfa\x3f\x55\xfd\xfa\xc1\x75\xc9\x2d\xfe\xf6\x3c\x6a\x96\x2e\xff\x79\x52\x06\x84\x6f\x50\xbb\x5b\x4b\xe8\xe5\xa9\x88\x97\x9c" },
	{ 1024 * 1024 * 4,          "\x0d\x24\xc8\xcb\x0f\xfa\x3f\x55\xfd\xfa\xc1\x75\xc9\x2d\xfe\xf6\xc3\x79\x1e\xed\x17\x26\x4b\x32\x96\x22\xee\x0f\x7a\x33\x51\xeb\x24\x36\x6a\x84" },
	{ 1024 * 1024 * 7,          "\x0d\x24\xc8\xcb\x0f\xfa\x3f\x55\xfd\xfa\xc1\x75\xc9\x2d\xfe\xf6\x24\xea\x81\x3d\x04\x0f\x24\x69\xae\x33\xfa\xc5\xb4\xde\xbb\xd1\x74\x53\xbb\xca" },
	{ 1024 * 1024 + 307200,     "\x0d\x24\xc8\xcb\x0f\xfa\x3f\x55\xfd\xfa\xc1\x75\xc9\x2d\xfe\xf6\xc3\xd8\xbe\x6d\x69\xa6\x93\x79\x24\xf6\x5f\x7e\x17\x18\xaa\x2d\x3e\xf1\x4e\xce" },
	{ 1024 * 1024 + 153600,     "\x0d\x24\xc8\xcb\x0f\xfa\x3f\x55\xfd\xfa\xc1\x75\xc9\x2d\xfe\xf6\x34\x05\xce\x69\x0c\xa3\xb8\x13\x3d\x4c\x4b\x31\x18\xb9\x21\x0b\xbb\x99\xb5\xf7" },
	{ 1024 * 1024 * 2 - 153600, "\x0d\x24\xc8\xcb\x0f\xfa\x3f\x55\xfd\xfa\xc1\x75\xc9\x2d\xfe\xf6\x00\x5f\xc0\xc6\xd9\xd5\x13\x8c\x83\x99\xc0\x68\xa6\x37\x03\x1b\x83\x47\xcb\xe5" },
	{ 1024 * 1024 * 2 - 307200, "\x0d\x24\xc8\xcb\x0f\xfa\x3f\x55\xfd\xfa\xc1\x75\xc9\x2d\xfe\xf6\xb9\xa2\xb4\xfc\xb7\xd5\xfd\xdc\xa0\x15\x85\x4b\x2f\x8d\xa5\x77\x1c\xa8\x80\xcf" },
	{ 1024 * 1024 * 2 + 307200, "\x0d\x24\xc8\xcb\x0f\xfa\x3f\x55\xfd\xfa\xc1\x75\xc9\x2d\xfe\xf6\x0a\x63\xa1\x97\xd5\x7e\x83\x14\x6e\x45\x6a\x40\x70\xf2\xc0\xa5\x87\x8c\xa8\x6b" },

};

static size_t test_chunks[] =
{
	0, /* filled in with entire size */
	2, /* placeholder for randomized chunk lengths */
/*
	1,
	7,
*/
	1024,
	4093,
	4096,
	32768,
	307200,
	307200 * 2,
	1024 * 1024,
	1024 * 1536,
	1024 * 1024 + 307200,
	1024 * 1024 - 307200,
	1024 * 1024 * 2 + 307200,
	1024 * 1024 * 2 - 307200
};

/*
#define CREATE_VECTOR_FILES
*/

int main (int argc, char *argv[])
{
	FSTHash *hash, *hash1;
	struct stat st;
	char *filename;
	uint8 *data;
	size_t len, off;
	int i, x;
	int all_ok;
	struct timeval start, stop;

	if (argc == 2)
	{
		if (stat (argv[1], &st) == -1)
		{
			printf ("Error: Cannot stat file \"%s\"\n", argv[1]);
			return 1;
		}

		if (!(hash = fst_hash_create ()))
			return 1;

		if (!fst_hash_file (hash, argv[1]))
		{
			printf ("Error: Unable to open file \"%s\"\n", argv[1]);
			fst_hash_free (hash);	
			return 1;
		}

		if((filename = strrchr (argv[1], '/')))
			filename++;
		else
			filename = argv[1];

		/* print file path */
		printf ("%s\n", argv[1]);

		/* print sig2dat link */
		printf ("  sig2dat:///|File:%s|Length:%uBytes|UUHash:=%s\n",
		        filename, (unsigned int)st.st_size, fst_hash_encode64_fthash (hash));

		/* print magnet uri */
		printf ("  magnet:?xt=urn:kzhash:%s&dn=%s\n",
				fst_hash_encode16_kzhash (hash), filename);

		fst_hash_free (hash);

		return 0;
	}

	printf ("\nNo file specified. Running self test.\n");
	printf ("Preparing data to hash...");

	/* init data we hash */
	len = test_vectors[0].len;
	for (i=0; i < sizeof (test_vectors) / sizeof (test_vectors[0]); i++)
		if (test_vectors[i].len > len)
			len = test_vectors[i].len;

	if (!(data = malloc (len + 4)))
		return 1;

	r250_init (1);
	for (i=0; i < (len+4)/4; i++)
		((uint32*)data)[i] = r250();

#ifdef CREATE_VECTOR_FILES
	/* write test vectors to files */
	for (i=0; i < sizeof (test_vectors) / sizeof (test_vectors[0]); i++)
	{
		char file[64];
		FILE *fp;

		sprintf (file, "kzhash%d.vec", test_vectors[i].len);

		if ((fp = fopen (file, "wb")))
		{
			fwrite (data, sizeof (uint8), test_vectors[i].len, fp);
			fclose (fp);
		}
	}
#endif

	printf ("DONE\n");

	/* run self test */
	hash = fst_hash_create ();
	all_ok = TRUE;
	
	/* get start time */
	gettimeofday (&start, NULL);

	for (i=0; i < sizeof (test_vectors) / sizeof (test_vectors[0]); i++)
	{
		hash1 = fst_hash_create_raw (test_vectors[i].kzhash, FST_KZHASH_LEN);
		printf ("\nTest len: %d\n", test_vectors[i].len);
		printf ("  reference:     %s\n", fst_hash_encode16_kzhash (hash1));
		fst_hash_free (hash1);
		
		test_chunks[0] = test_vectors[i].len;

		for (x=0; x < sizeof (test_chunks) / sizeof (test_chunks[0]); x++)
		{
			fst_hash_init (hash);

			len = test_chunks[x];

			if (len == 2)
			{
				/* randomized mode */
				printf ("  chunk random : ");
				r250_init (1);

				for (off=0; off < test_vectors[i].len; off += len)
				{
					len = r250 () % 0xFFFF;
					if(off + len > test_vectors[i].len)
						len = test_vectors[i].len - off;
					fst_hash_update (hash, data + off, len);
				}
			}
			else
			{
				printf ("  chunk %7d: ", test_chunks[x]);

				for (off=0; off < test_vectors[i].len; off += len)
				{
					if(off + len > test_vectors[i].len)
						len = test_vectors[i].len - off;
					fst_hash_update (hash, data + off, len);
				}
			}

			fst_hash_finish (hash);

			printf ("%s", fst_hash_encode16_kzhash (hash));

			if (!memcmp (FST_KZHASH (hash), test_vectors[i].kzhash, FST_KZHASH_LEN))
				printf ("  OK\n");
			else
			{
				printf ("  FAILED\n");
				all_ok = FALSE;
			}
		}
	}

	/* get stop time */
	gettimeofday (&stop, NULL);

	if (start.tv_usec > stop.tv_usec)
	{
		stop.tv_usec += 1000000;
		stop.tv_sec--;
	}

	stop.tv_usec -= start.tv_usec;
	stop.tv_sec -= start.tv_sec;

	if (all_ok)
		printf ("\nSummary: All tests completed successfully.\n");
	else
		printf ("\nSummary: One or more tests FAILED!\n");

	printf ("Elapsed time: %f seconds.\n",((double) stop.tv_sec) + ((double) stop.tv_usec / 1e6));
	printf ("WARNING: Self test only verified to work correctly on 32 bit little endian archs!\n\n");

	fst_hash_free (hash);
	free (data);

	return 0;
}

#endif /* HASH_TEST */

/*****************************************************************************/
