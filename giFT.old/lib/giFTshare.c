#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include "giFTpacket.h"
#include "giFTproto.h"
#include "giFTbuffer.h"
#include "giFTdescribe.h"
#include "giFTshare.h"

/**
 * The status of a shared file: whether we just added it, we just
 * removed it, or it's still here and has been around for a while.
 */
typedef enum { SHARE_STATUS_NONE = 0, SHARE_STATUS_NEW = 1,
    SHARE_STATUS_OLD = 2, SHARE_STATUS_NEWOLD = 3 } ShareStatus;

/**
 * A structure representing a single shared file.
 */
typedef struct s_SharedFile {
    char *filename;
    char *urlfilename;
    char *htmlfilename;
    time_t mtime;
    ShareStatus status;
    char checksum[11];
    char filesize[11];
    const char *contenttype;
    Buffer buf;
    struct s_SharedFile *next;
} SharedFile;

/**
 * The structure pointed to by a ShareHandle, which represents the list
 * of files being shared.
 */
struct s_ShareHandle {
    char *sharedir;
    SharedFile *head;
};

/**
 * Create a new ShareHandle, which references no files.
 */
ShareHandle shareNewHandle(const char *sharedir)
{
    ShareHandle h = malloc(sizeof(struct s_ShareHandle));
    if (!h) return NULL;

    if (sharedir) {
	h->sharedir = strdup(sharedir);
	if (!(h->sharedir)) {
	    free(h);
	    return NULL;
	}
    } else {
	h->sharedir = NULL;
    }
    h->head = NULL;

    return h;
}

/**
 * Free the memory allocated to a ShareHandle.
 */
void shareFreeHandle(ShareHandle *shp)
{
    ShareHandle h;
    SharedFile *p;
    
    if (!shp || !*shp) return;
    h = *shp;

    p = h->head;
    while(p) {
	SharedFile *nextp = p->next;
	free(p->filename);
	free(p->urlfilename);
	free(p->htmlfilename);
	bufferFree(&(p->buf));
	free(p);
	p = nextp;
    }
    if (h->sharedir) free(h->sharedir);
    free(h);
    *shp = NULL;
}

/**
 * Send the current list of shared files to the given Connection, which
 * is presumably a new one.
 */
int shareSendAll(ShareHandle h, Connection c)
{
    SharedFile *p;

    if (h == NULL || c == NULL) return -1;

    for(p = h->head; p; p = p->next) {
	if ((p->status & SHARE_STATUS_NEW) != 0 && p->buf.valid) {
	    writePacket(c, 0x04, p->buf.data, p->buf.pos);
	}
    }
    return 1;
}

/**
 * Like strdup, only URL-escape any weird chars.
 */
static char *urlify(const char *s)
{
    char *d;
    char *ret = malloc(3*strlen(s) + 1);
    char *shortret;
    if (!ret) return ret;

    d = ret;
    while(*s) {
	unsigned char c = *s++;
	if (c == ' ') {
	    *d = '+';
	    d += 1;
	} else if (strchr("%+/\\[]()<>'\"", c) || c < ' ' || c > '~') {
	    sprintf(d, "%%%02x", c);
	    d += 3;
	} else {
	    *d = c;
	    d += 1;
	}
    }
    *d = '\0';

    shortret = realloc(ret, strlen(ret)+1);
    if (shortret) ret = shortret;

    return ret;
}

/**
 * Like strdup, only HTML-escape any weird chars.
 */
static char *htmlify(const char *s)
{
    char *d;
    char *ret = malloc(6*strlen(s) + 1);
    char *shortret;
    if (!ret) return ret;

    d = ret;
    while(*s) {
	unsigned char c = *s++;
	if (c == '&') {
	    strcpy(d, "&amp;");
	    d += 5;
	} else if (c == '<') {
	    strcpy(d, "&lt;");
	    d += 4;
	} else if (c == '>') {
	    strcpy(d, "&gt;");
	    d += 4;
	} else if (c == '"') {
	    strcpy(d, "&quot;");
	    d += 6;
	} else if (c < ' ' || c > '~') {
	    sprintf(d, "&#%u;", c);
	    d += strlen(d);
	} else {
	    *d = c;
	    d += 1;
	}
    }
    *d = '\0';

    shortret = realloc(ret, strlen(ret)+1);
    if (shortret) ret = shortret;

    return ret;
}

/**
 * Fill a SharedFile structure with information about the given file.
 */
static int fill_SharedFile(SharedFile *sfp, char *pathname, struct stat *st)
{
    FileDescription fdesc;
    StatsFileType ftype;
    Buffer *bp;
    byte tbuf[2];
    FILE *fp = NULL;
    int res;
    int i;

    if (sfp == NULL || pathname == NULL || st == NULL) return -1;

    /* Open the file. */
    fp = fopen(pathname, "rb");
    if (!fp) return -1;

    res = describeFile(&fdesc, &ftype, &(sfp->contenttype), sfp->filename,
	    fp, st);
    fclose(fp);
    if (res < 0) return res;

    /* Write a binary representation of the FileDescription to the Buffer. */
    bp = &(sfp->buf);
    tbuf[0] = 0x00;
    tbuf[1] = ftype;
    bufferWriteData(bp, tbuf, 2);
    bufferWriteData(bp, fdesc.hash, 20);
    bufferWriteEncodedInt(bp, fdesc.checksum);
    bufferWriteEncodedInt(bp, fdesc.filesize);
    bufferWriteEncodedInt(bp, fdesc.ntags);
    for(i=0;i<fdesc.ntags;++i) {
	bufferWriteEncodedInt(bp, fdesc.tags[i].tag);
	bufferWriteEncodedInt(bp, fdesc.tags[i].len);
	bufferWriteData(bp, fdesc.tags[i].data, fdesc.tags[i].len);
    }

    sprintf(sfp->checksum, "%u", fdesc.checksum);
    sprintf(sfp->filesize, "%u", fdesc.filesize);

    return 0;
}

/**
 * The context for the send_update function.
 */
typedef struct {
    Connection *cs;
    SharedFile *sf;
} SharedSendContext;

/**
 * Send an update to a supernode.
 */
static int send_update(int which, struct sockaddr *addr, socklen_t addrlen,
	void *context)
{
    SharedSendContext *ctx = context;
    Connection *cs = ctx->cs;
    SharedFile *sf = ctx->sf;
    byte *buf = sf->buf.data;
    size_t buflen = sf->buf.pos;
    int type = 0x04;  /* Share file */

    if (sf->buf.valid == 0) return 0;
    if (sf->status == SHARE_STATUS_OLD) {
	type = 0x05;  /* Unshare file */
	/* Skip the 2-byte type indicator */
	buf += 2;
	buflen -= 2;
    }

    writePacket(cs[which], type, buf, buflen);
    return 0;
}

/**
 * Update the current list of shared files and send the changes to all
 * of the established Connections in the list.
 */
int shareUpdate(ShareHandle h, Connection *cs, size_t nconn)
{
    DIR *dirh = NULL;
    struct dirent *dent;
    SharedFile *p, **pp;
    int res;
    struct stat st;
    SharedSendContext ah_context;

    if (h == NULL || cs == NULL || h->sharedir == NULL) return -1;

    /* Mark each existing entry we've sent before as old. */
    for(p = h->head; p; p = p->next) {
	if ((p->status & SHARE_STATUS_NEW) != 0) {
	    p->status = SHARE_STATUS_OLD;
	}
    }

    /* Get each file in the directory. */
    dirh = opendir(h->sharedir);
    if (!dirh) return -1;

    while((dent = readdir(dirh)) != NULL) {
	char *pathname;

	/* Are we interested in this one? */
	pathname = malloc(strlen(h->sharedir) + 1 + strlen(dent->d_name) + 1);
	if (!pathname) return -1;
	sprintf(pathname, "%s/%s", h->sharedir, dent->d_name);
	res = stat(pathname, &st);
	if (res < 0 || !S_ISREG(st.st_mode)) {
	    /* It's not a regular file, so skip it. */
	    free(pathname);
	    continue;
	}

	/* See if we've got this one already.  Check both the filename
	 * and the last modification time. */
	for (p = h->head; p; p = p->next) {
	    if (!strcmp(p->filename, dent->d_name) &&
		    st.st_mtime <= p->mtime &&
		    p->status == SHARE_STATUS_OLD) {
		break;
	    }
	}
	if (!p) {
	    /* Nope, create it. */
	    p = malloc(sizeof(SharedFile));
	    if (!p) {
		free(pathname);
		return -1;
	    }
	    p->filename = strdup(dent->d_name);
	    if (!p->filename) {
		free(pathname);
		free(p);
		return -1;
	    }
	    p->urlfilename = urlify(p->filename);
	    if (!p->urlfilename) {
		free(p->filename);
		free(pathname);
		free(p);
		return -1;
	    }
	    p->htmlfilename = htmlify(p->filename);
	    if (!p->htmlfilename) {
		free(p->urlfilename);
		free(p->filename);
		free(pathname);
		free(p);
		return -1;
	    }
	    p->mtime = st.st_mtime;
	    p->status = SHARE_STATUS_NONE;
	    bufferNew(&p->buf);
	    p->next = h->head;
	    h->head = p;
	    res = fill_SharedFile(p, pathname, &st);
	    if (res < 0) {
		free(pathname);
		continue;
	    }
	    p->status = SHARE_STATUS_NEW;
	} else {
	    /* Hasn't changed since last time. */
	    p->status = SHARE_STATUS_NEWOLD;
	}

	free(pathname);
    }

    closedir(dirh);

    /* Update the supernodes.  We remove all the old entries before
     * sending new ones. */

    /* Go through the list, and remove any old entries. */
    for(p = h->head; p; p = p->next) {
	if (p->status == SHARE_STATUS_OLD) {
	    /* We just removed this one. */
	    ah_context.cs = cs;
	    ah_context.sf = p;
	    getConnecteds(cs, nconn, send_update, &ah_context);
	}
    }

    /* Go through the list, and add any new entries. */
    for(p = h->head; p; p = p->next) {
	if (p->status == SHARE_STATUS_NEW) {
	    /* We just removed this one. */
	    ah_context.cs = cs;
	    ah_context.sf = p;
	    getConnecteds(cs, nconn, send_update, &ah_context);
	}
    }

    /* Now purge any old entries. */
    pp = &(h->head);
    while(*pp) {
	if (((*pp)->status & SHARE_STATUS_NEW) == 0) {
	    /* Purge this one. */
	    p = *pp;
	    *pp = p->next;
	    free(p->filename);
	    free(p->urlfilename);
	    free(p->htmlfilename);
	    bufferFree(&(p->buf));
	    free(p);
	} else {
	    /* Go on to the next one. */
	    pp = &((*pp)->next);
	}
    }

    return 1;
}

/**
 * Return the eventual output length of this file list.  mode == 0 for a
 * textual output, 1 for a binary output.
 */
int shareGetTotalLength(ShareHandle sh, int mode)
{
    SharedFile *p;
    int total = 0;

    if (sh == NULL || mode < 0 || mode > 1) return -1;

    /* The fixed overhead for HTML. */
    if (mode == 0) {
	total = 6 + 6 + 7 + 2 + 8 + 7 + 7 + 2;
    }

    /* Walk the list, adding up the lengths. */
    for(p = sh->head ; p ; p = p->next) {
	if (mode == 0) {
	    total += 4 + 4 + 10 + strlen(p->checksum) + 1 +
		strlen(p->urlfilename) + 2 + strlen(p->htmlfilename) +
		4 + 4 + strlen(p->filesize) + 2;
	} else {
	    total += p->buf.pos - 2;
	}
    }

    return total;
}

/**
 * Get the length and type of the speficied file, and return a handle to
 * it, which will be passed back to shareWriteFile later (but before any
 * modifications to the ShareHandle can be done).
 */
void *shareGetFileLengthAndType(ShareHandle sh, const char *uri,
	int *clenp, const char **ctypep)
{
    SharedFile *p;

    if (clenp) *clenp = -1;
    if (ctypep) *ctypep = NULL;

    if (sh == NULL || uri == NULL) return NULL;

    /* Find the right file. */
    for(p = sh->head ; p ; p = p->next) {
	if (!strcmp(p->filename, uri)) {
	    /* Here we go. */
	    if (clenp) *clenp = atoi(p->filesize);
	    if (ctypep) *ctypep = p->contenttype;
	    return p;
	}
    }

    return NULL;
}

/**
 * Write a file list to the given Connection.  mode is as in
 * shareGetTotalLength, above.
 */
ConnectionState shareWriteFileList(ShareHandle sh, Connection c, int mode)
{
    SharedFile *p;

    if (sh == NULL || c == NULL || mode < 0 || mode > 1) {
	return CONNECTED_CLOSED;
    }

    /* If HTML, write a header. */
    if (mode == 0) {
	writeRawString(c, "<html><body><table>\r\n");
    }

    /* Walk the file list. */
    for(p = sh->head ; p ; p = p->next) {
	if (mode == 0) {
	    writeRawString(c, "<tr><td><a href=\"/");
	    writeRawString(c, p->checksum);
	    writeRawString(c, "/");
	    writeRawString(c, p->urlfilename);
	    writeRawString(c, "\">");
	    writeRawString(c, p->htmlfilename);
	    writeRawString(c, "</a><td>");
	    writeRawString(c, p->filesize);
	    writeRawString(c, "\r\n");
	} else {
	    writeRawData(c, p->buf.data+2, p->buf.pos-2, 0);
	}
    }

    /* If HTML, write a footer. */
    if (mode == 0) {
	writeRawString(c, "</table></body></html>\r\n");
    }

    return CONNECTED_PEER_FLUSHING;
}

/**
 * Arrange that the given portion of the given file is written to the
 * given Connection.
 */
ConnectionState shareWriteFile(ShareHandle sh, Connection c, void *handle,
	int rangestart, int length)
{
    FILE *fp;
    int res;
    char *pathname;
    SharedFile *sf = handle;

    if (sh == NULL || c == NULL || sf == NULL) {
	return CONNECTED_CLOSED;
    }

    if (length == 0) {
	return CONNECTED_PEER_FLUSHING;
    }

    /* Build the pathname. */
    pathname = malloc(strlen(sh->sharedir) + 1 + strlen(sf->filename) + 1);
    if (!pathname) return CONNECTED_CLOSED;
    sprintf(pathname, "%s/%s", sh->sharedir, sf->filename);
    fp = fopen(pathname, "rb");
    free(pathname);
    if (!fp) {
	return CONNECTED_CLOSED;
    }
    res = fseek(fp, rangestart, SEEK_SET);
    if (res < 0) {
	return CONNECTED_CLOSED;
    }

    res = startStreamFile(c, fp, length);
    if (res < 0) {
	return CONNECTED_CLOSED;
    }

    return CONNECTED_PEER_STREAMING;
}
