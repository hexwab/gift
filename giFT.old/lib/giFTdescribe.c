#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "giFTdescribe.h"
#include "giFTmd5.h"
#include "giFThash.h"

/**
 * The table used for determining type information for files based on
 * suffixes.
 */
const SuffixTableEntry describeSuffixTable[] = {
    { ".txt", "text/plain", STATS_DOCUMENTS, NULL, NULL },
    { ".gif", "image/gif", STATS_IMAGES, NULL, NULL },
    { ".jpg", "image/jpeg", STATS_IMAGES, NULL, NULL },
    { ".mp3", "audio/mpeg", STATS_AUDIO, NULL, NULL },
    { ".mpg", "video/mpeg", STATS_VIDEO, NULL, NULL },
    { NULL, NULL, 0, NULL, NULL }
};

/**
 * Add a tag to a given FileDescription.
 */
int describeAddDataTag(FileDescription *fdp, int tag, const byte *data,
	size_t len)
{
    byte *newdata;

    if (fdp == NULL) return -1;

    if (len > 0) {
	newdata = malloc(len);
	if (newdata == NULL) return -1;
	memmove(newdata, data, len);
    } else {
	newdata = NULL;
    }

    /* We allocate tags in batches of 10. */
    if (fdp->ntags % 10 == 0) {
	/* We need 10 more. */
	FileTag *newtags = realloc(fdp->tags, (fdp->ntags+10)*sizeof(FileTag));
	if (!newtags) {
	    if (newdata) free(newdata);
	    return -1;
	}
	fdp->tags = newtags;
    }
    fdp->tags[fdp->ntags].tag = tag;
    fdp->tags[fdp->ntags].len = len;
    fdp->tags[fdp->ntags].data = newdata;
    ++(fdp->ntags);
    return 0;
}

/**
 * Add an encoded integer as a tag to a given FileDescription.
 */
int describeAddIntTag(FileDescription *fdp, int tag, size_t v)
{
    byte buf[10];
    int pos = 10;
    byte lastflag = 0x00;
    
    if (fdp == NULL) return -1;
    do {
	buf[--pos] = lastflag | (v & 0x7f);
	v >>= 7;
	lastflag = 0x80;
    } while (v > 0);

    describeAddDataTag(fdp, tag, buf+pos, 10-pos);
    return 0;
}

/**
 * Add a resolution tag to a given FileDescription.
 */
int describeAddResolutionTag(FileDescription *fdp, size_t width,
	size_t height)
{
    byte buf[20];
    int pos = 20;
    byte lastflag = 0x00;
    
    if (fdp == NULL) return -1;
    do {
	buf[--pos] = lastflag | (height & 0x7f);
	height >>= 7;
	lastflag = 0x80;
    } while (height > 0);
    lastflag = 0;
    do {
	buf[--pos] = lastflag | (width & 0x7f);
	width >>= 7;
	lastflag = 0x80;
    } while (width > 0);

    describeAddDataTag(fdp, FILE_TAG_RESOLUTION, buf+pos, 20-pos);
    return 0;
}

/**
 * Describe a given file into a FileDescription structure.  Its type for
 * statistics purposes should be returned in *ftype.  Return 0 if
 * everything went well, -1 on error.
 */
int describeFile(FileDescription *fdp, StatsFileType *ftype,
	const char **contenttypep, const char *filename, FILE *fp,
	struct stat *st)
{
    const char *suffix;
    byte *filebuf;
    size_t firstlen;
    int res;
	struct MD5Context md5;
    const SuffixTableEntry *e;
    unsigned int smallhash;
    const size_t chunk_size = 307200;

    if (fdp == NULL || filename ==  NULL || fp == NULL || st == NULL) {
	return -1;
    }

    fdp->filesize = st->st_size;
    fdp->ntags = 0;
    fdp->tags = NULL;

    /* Calculate the long hash, the short hash, and the checksum. */
    firstlen = chunk_size;
    if (st->st_size < firstlen) firstlen = st->st_size;
    filebuf = malloc(firstlen);
    if (!filebuf) return -1;
    rewind(fp);
    res = fread(filebuf, 1, firstlen, fp);
    if (res < firstlen) {
	free(filebuf);
	return -1;
    }
    MD5Init(&md5);
    MD5Update(&md5, filebuf, firstlen);
    MD5Final(fdp->hash, &md5);

    /* Calculate the 4-byte small hash. */
    smallhash = 0xffffffff;

    if (fdp->filesize > chunk_size) {
	size_t offset = 0x100000;
	size_t lastpos = chunk_size;
	size_t endlen;
	while(offset+2*chunk_size < fdp->filesize) {
	    if (fseek(fp, offset, SEEK_SET) < 0 ||
		    fread(filebuf, 1, chunk_size, fp) < chunk_size) {
		free(filebuf);
		return -1;
	    }
	    smallhash = hashSmallHash(filebuf, chunk_size, smallhash);
	    lastpos = offset+chunk_size;
	    offset <<= 1;
	}
	endlen = fdp->filesize - lastpos;
	if (endlen > chunk_size) endlen = chunk_size;
	if (fseek(fp, fdp->filesize - endlen, SEEK_SET) < 0 ||
		fread(filebuf, 1, endlen, fp) < endlen) {
	    free(filebuf);
	    return -1;
	}
	smallhash = hashSmallHash(filebuf, endlen, smallhash);
    }

    smallhash ^= fdp->filesize;
    fdp->hash[16] = smallhash & 0xff;
    fdp->hash[17] = (smallhash >> 8) & 0xff;
    fdp->hash[18] = (smallhash >> 16) & 0xff;
    fdp->hash[19] = (smallhash >> 24) & 0xff;

    /* Finally, calculate the checksum. */
    fdp->checksum = hashChecksum(fdp->hash);
    free(filebuf);

    /* The only required tag is the filename. */
    res = describeAddDataTag(fdp, 0x02, filename, strlen(filename));
    if (res < 0) {
	if (fdp->tags) {
	    size_t i;
	    for(i=0;i<fdp->ntags;++i) {
		byte *p = fdp->tags[i].data;
		if (p) free(p);
	    }
	    free(fdp->tags);
	}
	fdp->tags = NULL;
	fdp->ntags = 0;
	return -1;
    }

    /* Find the extension on the filename.  We use this to determine its
     * type. */
    suffix = strrchr(filename, '.');
    if (!suffix) {
	/* We don't know what kind of file this is, so we're done. */
	if (ftype) *ftype = STATS_MISC;
	if (contenttypep) *contenttypep = "application/octet-stream";
	return 0;
    }

    for (e = describeSuffixTable; e->suffix; ++e) {
	if (!strcmp(e->suffix, suffix)) {
	    /* Here's our entry. */
	    if (ftype) *ftype = e->ftype;
	    if (contenttypep) *contenttypep = e->mimetype;
	    if (e->handler) {
		res = e->handler(fdp, filename, fp, st, e->context);
		if (res < 0) {
		    if (fdp->tags) {
			size_t i;
			for(i=0;i<fdp->ntags;++i) {
			    byte *p = fdp->tags[i].data;
			    if (p) free(p);
			}
			free(fdp->tags);
		    }
		    fdp->tags = NULL;
		    fdp->ntags = 0;
		    return -1;
		}
	    }
	    return 0;
	}
    }

    /* Didn't find a match. */
    if (ftype) *ftype = STATS_MISC;
    if (contenttypep) *contenttypep = "application/octet-stream";
    return 0;
}
