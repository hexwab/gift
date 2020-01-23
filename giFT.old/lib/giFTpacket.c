#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include "giFTcrypt.h"
#include "giFTpacket.h"

#define NUM_PINGS 4000

/**
 * The Connection data type is a pointer to a structure of this type (or
 * else is NULL).  These records encapsulate all the state needed to
 * maintain a non-blocking connection to a remote host.  It should not
 * be possible for a remote host to lock up the giFT daemon in any way.
 * Note that some of these fields are used for all kinds of Connections,
 * and some only have use to Connections in certain ConnectionStates.
 */
struct s_Connection {
    int fd;
    struct sockaddr *addr;
    socklen_t addrlen;
    struct sockaddr_in server_addr;
    time_t expiry;
    int reconnect;
    char *username, *networkname;
    CipherState wcs, rcs;
    SupernodeStats stats;
    ConnectionState connectionstate;
    byte headerbuf[5];
    size_t headerread;
    int packettype;
    byte *packetbuf;
    size_t packetsize, packetread;
    byte *writebuf;
    size_t writebufalloc, writebufsize, writebufwritten;
    unsigned int xinu_state_r, xinu_state_w;
    unsigned short qid;
    size_t refcount;
    byte *slurpbuf;
    size_t slurpsize, slurpread;
    byte decryptbuf[NUM_PINGS];
    size_t decryptread;
    int outputskip, outputmax;
    FILE *streamfp;
    DownloadContext dlcontext;
    size_t dlid;
    int dlgettingdata;
    size_t dlpos;
    size_t dlwaittime;
    int dlretries;
};

/**
 * An array of strings with human-readable versions of the
 * ConnectionStates.
 */
const char *connectionStateName[NumConnectionState] = {
    "NULL", "NODE_CONNECTING", "NODE_CONNECTED", "SUPERNODE_CONNECTING",
    "SUPERNODE_WAITING", "SUPERNODE_SLURPING", "SUPERNODE_DECRYPTING",
    "SUPERNODE_CONNECTED", "CLIENT_READING", "CLIENT_WRITING",
    "CLIENT_FLUSHING", "PEER_READING", "PEER_WRITING", "PEER_FLUSHING",
    "PEER_STREAMING", "DOWNLOAD_CONNECTING", "DOWNLOAD_READING",
    "DOWNLOAD_WAITING", "CLOSED", "DEAD" };

/* We get known cipher states by looking at transcripts of
 * communications (in either direction), and breaking the
 * cipher.  Here's one.  The first byte of the header is always
 * 0x80.  After that there's four bytes indicating the key.
 * The known_cipher_state is the 64 byte internal state that
 * would be calculated as the state generation function of the key,
 * if only we knew what it was. */
static byte known_header[5] = { 0x80, 0x92, 0x63, 0x5b, 0xe1 };
static CipherState known_cipher_state = { 0x29, {
	0xF5, 0x90, 0x43, 0xA6, 0x60, 0x5F, 0x16, 0x2E,
	0xA7, 0x40, 0xB1, 0xC1, 0x53, 0x23, 0x91, 0x66,
	0xA0, 0x31, 0xB3, 0x58, 0xAF, 0xF9, 0x15, 0x05,
	0xCC, 0x7D, 0x2F, 0x8D, 0xA6, 0x13, 0x44, 0x38,
	0xE3, 0x7A, 0xE1, 0x53, 0x4C, 0x27, 0x6B, 0xA8,
	0x15, 0x2B, 0xBC, 0x2A, 0xED, 0xA1, 0xFD, 0xC5,
	0xC4, 0xC3, 0xEF, 0x4B, 0x8D, 0x7F, 0x82, 0x1D,
	0x0B, 0x23, 0x15, 0xE9, 0xDF, 0xDA, 0x9D
}};

/**
 * Make a non-blocking socket and connect it to the given address.
 */
static int makeNBSocket(struct sockaddr *addr, socklen_t addrlen)
{
    int s;
    int res;
    unsigned int flags;

    /* Make the socket */
    s = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s < 0) {
	return -1;
    }

    /* Make it non-blocking */
    fcntl(s, F_GETFL, &flags);
    if (fcntl(s, F_SETFL, flags | O_NONBLOCK) < 0) {
	close(s);
	return -1;
    }

    /* Start the connection.  Note that we expect the result to be -1,
     * and errno to equal EINPROGRESS, since this is a non-blocking
     * socket.  But if the connection happens to succeed right away
     * (without blocking), that's OK too.  */
    res = connect(s, addr, addrlen);
    if (res < 0 && errno != EINPROGRESS) {
	close(s);
	return -1;
    }

    return s;
}

/**
 * Initialize a Connection structure.
 */
static void initConnection(Connection c, ConnectionState state)
{
    c->fd = -1;
    c->expiry = 0;
    c->server_addr.sin_family = 0;
    c->server_addr.sin_port = 0;
    c->server_addr.sin_addr.s_addr = 0;
    c->reconnect = 0;
    c->addr = NULL;
    c->addrlen = 0;
    c->username = NULL;
    c->networkname = NULL;
    c->connectionstate = state;
    c->stats.lastheard = 0;
    c->headerread = 0;
    c->packetbuf = NULL;
    c->packetsize = 0;
    c->packetread = 0;
    c->writebuf = NULL;
    c->writebufalloc = 0;
    c->writebufsize = 0;
    c->writebufwritten = 0;
    c->qid = 0;
    c->refcount = 0;
    c->xinu_state_w = 0;
    c->xinu_state_r = 0;
    c->slurpbuf = NULL;
    c->slurpsize = 0;
    c->slurpread = 0;
    c->decryptread = 0;
    c->outputskip = 0;
    c->outputmax = -1;
    c->streamfp = NULL;
    c->dlid = 0;
    c->dlgettingdata = 0;
    c->dlpos = 0;
    c->dlwaittime = 0;
    c->dlretries = 0;
}

/**
 * Initiate a connection with a node.
 */
static Connection startConnection(struct sockaddr *nodeaddr, socklen_t addrlen,
	char *username, char *networkname, struct sockaddr_in *server_addr)
{
    int s;
    Connection retval = NULL;
    struct sockaddr *copyaddr = NULL;
    time_t now = time(NULL);

    /* Copy the username and networkname */
    if (username == NULL) username = "giFTuser";
    username = strdup(username);
    if (username == NULL) {
	return NULL;
    }
    if (networkname == NULL) networkname = "giFT";
    networkname = strdup(networkname);
    if (networkname == NULL) {
	free(username);
	return NULL;
    }

    /* Copy the dest address */
    copyaddr = malloc(addrlen);
    if (copyaddr == NULL) {
	free(username);
	free(networkname);
	return NULL;
    }
    memmove(copyaddr, nodeaddr, addrlen);

    /* Make the socket and start the connection */
    s = makeNBSocket(nodeaddr, addrlen);
    if (s < 0) {
	free(username);
	free(networkname);
	free(copyaddr);
	return NULL;
    }

    /* Now make the Connection handle and return it */
    retval = malloc(sizeof(struct s_Connection));
    if (retval == NULL) {
	close(s);
	free(username);
	free(networkname);
	free(copyaddr);
	return NULL;
    }

    initConnection(retval, CONNECTED_NODE_CONNECTING);
    retval->fd = s;
    retval->expiry = now + TIMEOUT_CONNECTING;
    if (server_addr) {
	retval->server_addr = *server_addr;
    }
    retval->addr = copyaddr;
    retval->addrlen = addrlen;
    retval->username = username;
    retval->networkname = networkname;
    retval->xinu_state_w = 0x51;
    retval->xinu_state_r = 0x53;

    return retval;
}

/**
 * Mark a Connection to go into a CONNECTED_DOWNLOAD_WAITING state, from
 * which we'll reconnect to the same host later.
 */
static void dl_waiting(Connection c, int which, ConnectionChangedHandler cch,
	void *cch_context)
{
    ConnectionState oldstate;

    if (c == NULL) return;

    /* Close the socket. */
    if (c->fd >= 0) {
	close(c->fd);
	c->fd = -1;
    }
    oldstate = c->connectionstate;
    c->connectionstate = CONNECTED_DOWNLOAD_WAITING;
    cch(which, oldstate, c->connectionstate, cch_context);
}

/**
 * Once we have been informed that the connect() has completed, we call
 * this routine to start the protocol.
 */
static int connectConnection(Connection c, int which,
	DownloadHandler dh, void *dh_context,
	ConnectionChangedHandler cch, void *cch_context)
{
    struct sockaddr_in myname;
    socklen_t namelen = sizeof(myname);
    byte *outmsg = NULL;
    unsigned int myaddr;
    unsigned int didconnect;
    socklen_t didlen = sizeof(didconnect);
    size_t userlen;
    char *username;
    ConnectionState oldstate;
	
    if (c==NULL || c->fd < 0) return -1;

    /* Check if the connection succeeded. */
    if (getsockopt(c->fd, SOL_SOCKET, SO_ERROR, &didconnect, &didlen) < 0
	    || didconnect != 0
	    || getsockname(c->fd, (struct sockaddr *)&myname, &namelen) < 0) {
	/* Nope. */
	if (c->connectionstate == CONNECTED_DOWNLOAD_CONNECTING) {
	    /* We get a "Connection refused" from a peer when we
	     * attempted to download a file from it.  Retry a few times
	     * before giving up.  Use increasing timeouts. */
	    static const size_t retry_timeouts[] = { 3, 5, 10, 20, 40 };
	    int numtries = c->dlretries;
	    ++(c->dlretries);
	    if (numtries <
		    (sizeof(retry_timeouts)/sizeof(retry_timeouts[0]))) {
		c->dlwaittime = retry_timeouts[numtries];
		dl_waiting(c, which, cch, cch_context);
		return 1;
	    }
	}
	/* We want to give up on this host. */
	close(c->fd);
	c->fd = -1;
	oldstate = c->connectionstate;
	c->connectionstate = CONNECTED_DEAD;
	cch(which, oldstate, c->connectionstate, cch_context);
	return 1;
    }
    myaddr = ntohl(c->server_addr.sin_addr.s_addr);
    if (myaddr == INADDR_ANY) myaddr = ntohl(myname.sin_addr.s_addr);

    /* We have an open (and non-blocking) TCP connection, which is
     * ready for writing. */

    /* Are we connecting to a node, a supernode, or a download attempt? */
    if (c->connectionstate == CONNECTED_NODE_CONNECTING) {
	/* This is a connection to a node.  Ask it who its supernode is. */
	writeRawData(c, "GET / HTTP/1.0\r\n\r\n", 18, 0);
	if (c->packetbuf) free(c->packetbuf);
	c->packetbuf = malloc(2001);
	if (c->packetbuf == NULL) {
	    fprintf(stderr, "Out of memory at %s:%d\n", __FILE__, __LINE__);
	    return -1;
	}
	c->packetbuf[2000] = '\0';
	c->packetsize = 2000;
	c->packetread = 0;

	oldstate = c->connectionstate;
	c->connectionstate = CONNECTED_NODE_CONNECTED;
	cch(which, oldstate, c->connectionstate, cch_context);
	return 1;
    } else if (c->connectionstate == CONNECTED_DOWNLOAD_CONNECTING) {
	size_t start, len;
	int res;
	char *uri;
	char range[40];

	/* This is a connection to a peer in order to download something. */

	/* Reset the retry counter. */
	c->dlretries = 0;

	/* Figure out what file and range to ask for. */
	res = dh(which, c->dlcontext, c->dlid, &uri, &start, &len, NULL,
		dh_context);
	if (res < 0) return res;

	if (c->packetbuf) free(c->packetbuf);
	c->packetbuf = malloc(16001);
	if (c->packetbuf == NULL) {
	    fprintf(stderr, "Out of memory at %s:%d\n", __FILE__, __LINE__);
	    return -1;
	}
	c->packetbuf[16000] = '\0';
	c->packetsize = 16000;
	c->packetread = 0;
	c->dlpos = start;

	writeRawString(c, "GET ");
	writeRawString(c, uri);
	writeRawString(c, " HTTP/1.0\r\n");
	sprintf(range, "Range: bytes=%u-%u\r\n", start, start + len - 1);
	writeRawString(c, range);
	writeRawString(c, "\r\n");

	oldstate = c->connectionstate;
	c->connectionstate = CONNECTED_DOWNLOAD_READING;
	cch(which, oldstate, c->connectionstate, cch_context);
	return 1;
    }

    /* This is a supernode we're now connected to.  Start the protocol. */

    /* Create a known client-side (writing) cipher state. */
    memmove(&(c->wcs), &known_cipher_state, sizeof(known_cipher_state));

    /* Create the message we're about to send */

    username = c->username;
    userlen = strlen(username);

    outmsg = malloc(8+userlen);
    if (outmsg == NULL) return -1;

    /* 4 bytes: IP address */
    outmsg[0] = (myaddr>>24) & 0xff;
    outmsg[1] = (myaddr>>16) & 0xff;
    outmsg[2] = (myaddr>>8) & 0xff;
    outmsg[3] = (myaddr) & 0xff;
    /* 2 bytes: port number we're listening on */
    outmsg[4] = ntohs(c->server_addr.sin_port) >> 8;
    outmsg[5] = ntohs(c->server_addr.sin_port) & 0xff;
    /* This next byte represents the user's advertised bandwidth, on
     * a logarithmic scale.  0xd1 represents "infinity" (actually,
     * 1680 kbps).  The value is approximately 14*log_2(x)+59, where
     * x is the bandwidth in kbps. */
    outmsg[6] = 0xd1;
    /* 1 byte: dunno. */
    outmsg[7] = 0x00;
    /* The rest: username (with no NUL at the end) */
    memmove(outmsg+8, username, userlen);

    /* Send the known header */
    writeRawData(c, known_header, 5, 0);
    writePacket(c, 2, outmsg, 8+userlen);

    free(outmsg);

    oldstate = c->connectionstate;
    c->connectionstate = CONNECTED_SUPERNODE_WAITING;
    cch(which, oldstate, c->connectionstate, cch_context);
    return 1;
}

/**
 * Close a Connection and free all allocated state associated with it.
 */
void closeConnection(Connection *cp)
{
    Connection c;

    if (cp == NULL) return;
    c = *cp;
    if (c == NULL) return;
    if (c->fd >= 0) close(c->fd);
    if (c->addr) free(c->addr);
    if (c->username) free(c->username);
    if (c->networkname) free(c->networkname);
    if (c->packetbuf) free(c->packetbuf);
    if (c->writebuf) free(c->writebuf);
    if (c->slurpbuf) free(c->slurpbuf);
    if (c->streamfp) fclose(c->streamfp);
    free(c);
    *cp = NULL;
}

/**
 * Enqueue data for later writing to the socket.  Set needs_crypt to 1
 * if we need to encrypt it before writing it.  Set needs_crypt to 0
 * if this data isn't supposed to be encrypted at all.  You shouldn't be
 * passing already encrypted data to this function.
 */
void writeRawData(Connection c, const byte *buf, size_t len, int needs_crypt)
{
    byte *newbuf;

    if (c == NULL) return;

    /* See if we're supposed to skip some of the output. */
    if (c->outputskip > 0) {
	size_t amt = c->outputskip;
	if (amt > len) amt = len;
	c->outputskip -= amt;
	buf += amt;
	len -= amt;
    }

    /* See if we have a cap on the amount of output we should write. */
    if (c->outputmax >= 0) {
	if (len > c->outputmax) len = c->outputmax;
	c->outputmax -= len;
    }

    /* Is there anything left to write? */
    if (len == 0) return;

    /* Make sure we've allocated enough memory to store the new data. */
    if (c->writebufsize + len > c->writebufalloc) {
	/* We don't have enough.  Allocate enough, and 4K more while
	 * we're at it. */
	newbuf = realloc(c->writebuf, c->writebufsize + len + 4096);
	if (newbuf == NULL) {
	    fprintf(stderr, "Out of memory at %s:%d\n", __FILE__, __LINE__);
	    return;
	}
	c->writebuf = newbuf;
	c->writebufalloc = c->writebufsize + len + 4096;
    }

    /* Copy the new data into our buffer, and encrypt it if necessary. */
    memmove(c->writebuf + c->writebufsize, buf, len);
    if (needs_crypt) {
	do_crypt(&(c->wcs), c->writebuf + c->writebufsize, len);
    }
    c->writebufsize += len;
}

/**
 * Enqueue an unencrypted string for later writing to the socket.
 */
void writeRawString(Connection c, const char *s)
{
    writeRawData(c, s, strlen(s), 0);
}

/**
 * Enqueue an entire packet (header and data) for later writing to a
 * socket.
 */
void writePacket(Connection c, int packet_type, byte *packet_data,
	size_t packet_len)
{
    byte header[5];
    byte type, hi, lo;
    int xtype;

    if (c == NULL) return;

    /* The first thing we have to do is figure out what the
     * byte-ordering of the header should look like */
    type = (packet_type) & 0xff;
    hi = (packet_len >> 8) & 0xff;
    lo = packet_len & 0xff;

    header[0] = 0x4b;
    xtype = c->xinu_state_w % 3;
    switch(xtype) {
	case 0:
	    header[1] = type;
	    header[2] = 0;
	    header[3] = hi;
	    header[4] = lo;
	    break;
	case 1:
	    header[1] = 0;
	    header[2] = hi;
	    header[3] = type;
	    header[4] = lo;
	    break;
	case 2:
	    header[1] = 0;
	    header[2] = lo;
	    header[3] = hi;
	    header[4] = type;
	    break;
    }

    /* And don't forget to update the xinu state. */
    c->xinu_state_w ^= ~(packet_len + type);

    /* Now enqueue the header and the data. */
    writeRawData(c, header, 5, 1);
    writeRawData(c, packet_data, packet_len, 1);
}

/**
 * Send any data that has been queued for writing.
 */
static int writeToConnection(Connection c)
{
    int res = 0;

    if (c == NULL) return 0;

    if (c->writebufwritten < c->writebufsize) {
	res = write(c->fd, c->writebuf + c->writebufwritten,
		c->writebufsize - c->writebufwritten);
	if (res <= 0) return res;
	c->writebufwritten += res;

	/* Was that everything? */
	if (c->writebufwritten == c->writebufsize) {
	    c->writebufwritten = 0;
	    c->writebufsize = 0;
	    c->writebufalloc = 0;
	    free(c->writebuf);
	    c->writebuf = NULL;
	}
    }
    return res;
}

/**
 * Stream data from a FILE* to the given Connection.
 */
static int streamToConnection(Connection c)
{
    int res;

    if (c == NULL) return 0;

    res = writeToConnection(c);
    if (res <= 0) return res;

    /* Have we written the whole last chunk? */
    if (c->writebufsize == 0) {
	/* Queue up some more, unless we've written all that was asked
	 * for. */
	if (c->outputmax == 0) {
	    /* That's all we should write. */
	    return 0;
	} else if (c->streamfp != NULL) {
	    static byte temp[8192];
	    int r = fread(temp, 1, sizeof(temp), c->streamfp);
	    if (r > 0) {
		/* Queue up the next block. */
		writeRawData(c, temp, r, 0);
	    }
	}
    }

    return res;
}

/**
 * Get a FILE* ready for streaming to a given Connection.  We start
 * streaming from its current file position, for up to length bytes.
 */
int startStreamFile(Connection c, FILE *fp, size_t length)
{
    if (c == NULL || fp == NULL) return -1;

    if (c->streamfp != NULL) {
	fclose(c->streamfp);
    }
    c->streamfp = fp;
    c->outputmax = length;

    if (length > 0) {
	static byte temp[8192];
	int r = fread(temp, 1, sizeof(temp), c->streamfp);
	if (r > 0) {
	    /* Queue up the first block. */
	    writeRawData(c, temp, r, 0);
	} else {
	    return -1;
	}
    }
    return 0;
}

/**
 * Read the response to a GET / HTTP/1.0 from a node (which may just
 * happen to be a supernode).
 */
static int readNodeResponse(Connection c, int which,
	ConnectionChangedHandler cch, void *cch_context)
{
    int res;
    char *eoh;
    char *supip;
    ConnectionState oldstate;

    if (c == NULL || c->fd < 0) return -1;

    res = read(c->fd, c->packetbuf + c->packetread,
	    c->packetsize - c->packetread);
    if (res <= 0) return res;
    c->packetread += res;
    c->packetbuf[c->packetread] = '\0';

    /* See if we've gotten the whole header yet */
    eoh = strstr(c->packetbuf, "\n\n");
    if (!eoh) eoh = strstr(c->packetbuf, "\r\n\r\n");

    if (!eoh) {
	/* Nope; get more later. */
	return res;
    }

    /* Chop off after the header */
    *eoh = '\0';

    /* Find the supernode address, if present */
    supip = strstr(c->packetbuf, "X-Kazaa-SupernodeIP: ");
    if (!supip) supip = strstr(c->packetbuf, "X-giFT-SupernodeIP: ");
    if (supip) {
	/* This wasn't a supernode, but it'll tell us who is. */
	unsigned int a1,a2,a3,a4,p;
	struct sockaddr_in sin;
	socklen_t sinlen = sizeof(sin);
	int scanres;

	scanres = sscanf(supip+21, "%u.%u.%u.%u:%u", &a1, &a2, &a3, &a4, &p);
	if (scanres == 5) {
	    sin.sin_family = AF_INET;
	    sin.sin_port = htons(p);
	    sin.sin_addr.s_addr = htonl(
		    (a1<<24) | (a2<<16) | (a3<<8) | a4);
	    if (c->addrlen < sinlen) {
		struct sockaddr *newaddr = realloc(c->addr, sinlen);
		if (newaddr == NULL) {
		    fprintf(stderr, "Out of memory at %s:%d\n",
			    __FILE__, __LINE__);
		} else {
		    c->addr = newaddr;
		    c->addrlen = sinlen;
		}
	    }
	    memmove(c->addr, &sin, c->addrlen);
	}
    }

    /* OK, c->addr now should contain the address of a supernode.
     * Try to contact it. */
    close(c->fd);
    c->fd = makeNBSocket(c->addr, c->addrlen);
    oldstate = c->connectionstate;
    if (c->fd < 0) {
	c->connectionstate = CONNECTED_DEAD;
    } else {
	c->connectionstate = CONNECTED_SUPERNODE_CONNECTING;
    }
    cch(which, oldstate, c->connectionstate, cch_context);

    return res;
}

/**
 * Handle a received packet either internally, or by passing it back to
 * a PacketHandler.
 */
static int handle_packet(Connection c, int which, int type, size_t size,
	byte *buf, PacketHandler ph, void *ph_context)
{
    if (type == 0x09 && size == 60) {
	int i;

	/* We can handle packets of type 9 (statistics) right here */

	c->stats.lastheard = time(NULL);

	/* The first four bytes are the number of users */
	c->stats.numUsers = (buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|buf[3];

	/* The next 7 groups of 8 bytes are of the form:
	 * 4 bytes: number of files
	 * 2 bytes: mantissa of total file size
	 * 2 bytes: exponent of total file size
	 */
	for(i=0;i<7;++i) {
	    size_t mantissa;
	    int exponent;
	    size_t nf, gb;

	    nf = (buf[8*i+4]<<24)|(buf[8*i+5]<<16)|(buf[8*i+6]<<8)|
		buf[8*i+7];
	    mantissa = (buf[8*i+8]<<8)|buf[8*i+9];
	    exponent = (buf[8*i+10]<<8)|buf[8*i+11];

	    /* Convert to gigabytes */
	    if (exponent >= 30) {
		gb = mantissa << (exponent-30);
	    } else {
		gb = mantissa >> (30-exponent);
	    }

	    if (i == 0) {
		/* The first group are the totals. */
		c->stats.numTotFiles = nf;
		c->stats.numTotGBytes = gb;
	    } else {
		/* The other groups correspond to individual file types. */
		c->stats.numFiles[i-1] = nf;
		c->stats.numGBytes[i-1] = gb;
	    }
	}

	/* But we'll fall through and upcall anyway, in case the main
	 * program wants to do anything when we get a stats packet. */
    }
    return ph(which, type, size, buf, ph_context);
}

/**
 * Report the statistics from an active Connection.
 */
int reportStatsFromConnection(Connection c, SupernodeStats *s)
{
    if (c == NULL || s == NULL ||
	    (c->connectionstate != CONNECTED_SUPERNODE_CONNECTED
	     && c->connectionstate != CONNECTED_SUPERNODE_DECRYPTING)
	    || c->stats.lastheard == 0) {
	return -1;
    }
    memmove(s, &(c->stats), sizeof(c->stats));
    return 0;
}

/**
 * Read and decrypt from a Connection.
 */
static int readFromConnection(Connection c, int which, PacketHandler ph,
	void *ph_context, ConnectionChangedHandler cch, void *cch_context)
{
    int res;
    CipherState *rcs;

    if (c == NULL) return 0;

    if (c->connectionstate == CONNECTED_SUPERNODE_CONNECTED) {
	rcs = &(c->rcs);
    } else {
	rcs = NULL;
    }

    /* Where are we within the packet? */
    if (c->headerread == 0) {
	/* Read the tag byte */
	res = read(c->fd, c->headerbuf, 1);
	if (res <= 0) return res;
	if (rcs) {
	    do_crypt(rcs, c->headerbuf, 1);
	} else {
	    c->headerbuf[0] = 0x4b;
	}

	/* What kind of packet? */
	if (c->headerbuf[0] == 0x50) {
	    /* This was a ping packet.  Send back a pong. */
	    c->headerbuf[0] = 0x52;
	    writeRawData(c, c->headerbuf, 1, 1);
	} else if (c->headerbuf[0] == 0x4b) {
	    /* This is a regular packet.  Remember that so that we read
	     * the headers. */
	    c->headerread = 1;
	}
	return 1;
    } else if (c->headerread < 5) {
	/* Read the rest of the header */
	res = read(c->fd, c->headerbuf+(c->headerread), 5-(c->headerread));
	if (res <= 0) return res;
	if (rcs) {
	    do_crypt(rcs, c->headerbuf+(c->headerread), res);
	} else {
	    c->headerbuf[1] = 0x00;
	    c->headerbuf[2] = 0x40;
	    c->headerbuf[3] = 0x06;
	    c->headerbuf[4] = 0x02;
	}
	c->headerread += res;

	/* Do we have the whole header yet? */
	if (c->headerread == 5) {
	    /* Intuit the packet type and size */
	    byte *buf = c->headerbuf + 1;
	    int type = 0;
	    int len = 0;
	    int zero = -1;
	    int xtype = c->xinu_state_r % 3;
	    unsigned int head =
		(buf[0]<<24) | (buf[1]<<16) | (buf[2]<<8) | buf[3];

	    /* We're no longer sure of the correct xinu state for
	     * reading packets, since we didn't see the first bunch.
	     * We try to figure it out from context. */
	    if (head == 0x08000002 || head == 0x0900003c) {
		xtype = 0;
	    } else if (head == 0x00000802 || head == 0x0000093c) {
		xtype = 1;
	    } else if (head == 0x00020008 || head == 0x003c0009) {
		xtype = 2;
	    } else if ((head & 0xffff0000) == 0x07000000) {
		xtype = 0;
	    } else if ((head & 0xff00ff00) == 0x00000700) {
		xtype = 1;
	    } else if ((head & 0xff0000ff) == 0x00000007) {
		xtype = 2;
	    } else if ((head & 0xffff0000) == 0x00000000) {
		xtype = 0;
	    } else if ((head & 0xff00ff00) == 0x00000000) {
		xtype = 1;
	    } else if ((head & 0xff0000ff) == 0x00000000) {
		xtype = 2;
	    }

	    switch(xtype) {
		case 0:
		    type = buf[0];
		    zero = buf[1];
		    len = (buf[2] << 8) | buf[3];
		    break;
		case 1:
		    type = buf[2];
		    zero = buf[0];
		    len = (buf[1] << 8) | buf[3];
		    break;
		case 2:
		    type = buf[3];
		    zero = buf[0];
		    len = (buf[2] << 8) | buf[1];
		    break;
	    }

	    /* Update the xinu state */
	    c->xinu_state_r ^= ~(type+len);

	    /* Sanity check that the byte that should be 0 in fact is. */
	    if (zero != 0) return -1;

	    c->packettype = type;
	    c->packetsize = len;
	    c->packetread = 0;
	    if (c->packetbuf) free(c->packetbuf);
	    c->packetbuf = malloc(len+1);
	    if (c->packetbuf == NULL) {
		fprintf(stderr, "Out of memory at %s:%d\n",
			__FILE__, __LINE__);
	    }
	    /* Stick a NUL at the end of the buffer in case the packet
	     * handler might do some str*() calls. */
	    c->packetbuf[len] = '\0';

	    /* If it was a 0-length packet, then we're in fact done now. */
	    if (len == 0) {
		handle_packet(c, which, c->packettype, c->packetsize,
			c->packetbuf, ph, ph_context);
		c->headerread = 0;
	    }
	}
	return res;
    } else {
	/* Try to read more of the packet */
	res = read(c->fd, c->packetbuf + c->packetread,
		c->packetsize - c->packetread);
	if (res <= 0) return res;
	if (rcs) {
	    do_crypt(rcs, c->packetbuf + c->packetread, res);
	}
	c->packetread += res;

	/* Do we have it all yet? */
	if (c->packetread == c->packetsize) {
	    /* We have a complete packet; call the packet handler */

	    if (rcs == NULL) {
		/* We don't yet have a cipher state for input; try to
		 * get one by computing it from this packet. */
		res = decrypt_node_list(c->packetbuf, c->packetsize,
			&(c->rcs));
		if (res > 0) {
		    ConnectionState oldstate = c->connectionstate;

		    /* We're now OK to receive arbitrary packets. */
		    c->connectionstate = CONNECTED_SUPERNODE_CONNECTED;
		    cch(which, oldstate, c->connectionstate, cch_context);

		    /* Send our network name. */
		    writePacket(c, 0x1d, c->networkname,
			    strlen(c->networkname));

		    /* We successfully decrypted at least the last part
		     * of the node list.  Treat that part as a received
		     * packet. */
		    handle_packet(c, which, c->packettype, res & ~7,
			    c->packetbuf + c->packetsize - res,
			    ph, ph_context);
		} else {
		    res = 1;
		}
	    } else {
		handle_packet(c, which, c->packettype, c->packetsize,
			c->packetbuf, ph, ph_context);
	    }

	    /* Get ready for the next packet */
	    c->headerread = 0;
	}
	return res;
    }
}

/**
 * Read a query from a client connection, and pass it back via a
 * QueryHandler when it's completed.  A query starts with "<" and ends
 * with "/>".
 */
static int readFromClient(Connection c, int which, QueryHandler qh,
	void *qh_context, ConnectionChangedHandler cch, void *cch_context)
{
    int res;
    size_t l;
    byte *end;
    ConnectionState oldstate, newstate;
    char *version = "<giFT VERSION=\"1\"/>\n";

    if (c == NULL || c->fd < 0) return -1;

    res = read(c->fd, c->packetbuf + c->packetread,
	    c->packetsize - c->packetread);
    if (res <= 0) return res;
    c->packetread += res;
    c->packetbuf[c->packetread] = '\0';

    /* See if we've gotten the whole query yet */
    l = c->packetread;
    if (l > 1) l = 1;
    if (strncasecmp(c->packetbuf, "<", l)) {
	/* This didn't even start like a query! */
	return -1;
    }
    if (c->packetread <= 1) return res;
    
    end = strstr(c->packetbuf+1, "/>");
    if (!end) {
	/* We haven't yet read the whole query */
	return res;
    }
    end[2] = '\0';

    /* OK, we've got it all.  Mark the client connection as ready for
     * writing, and write the client protocol version number. */
    oldstate = c->connectionstate;
    c->connectionstate = CONNECTED_CLIENT_WRITING;
    cch(which, oldstate, c->connectionstate, cch_context);
    writeRawData(c, version, strlen(version), 0);
    
    /* Handle the query */
    newstate = qh(which, c->packetbuf, end+2-(c->packetbuf), qh_context);
    if (newstate != c->connectionstate) {
	oldstate = c->connectionstate;
	c->connectionstate = newstate;
	cch(which, oldstate, c->connectionstate, cch_context);
    }

    return res;
}

/**
 * Update the arguments to a select() call to properly reflect a set of
 * Connections. *np, readfds, writefds, exceptfds, tv should all have
 * sane initial values that would properly serve as arguments to
 * select().  Note that tv should not be NULL; make it some large value
 * if you have nothing else to do; this routine will set it to a smaller
 * value, depending on the Connection expiry times.  This routine will
 * add the appropriate bits to the fd_sets (and update *np) depending on
 * what the various Connections need to be doing.  This function will
 * return the number of Connections in the array which are selected
 * for any kind of activity.
 */
size_t selectSetupConnections(Connection *cs, size_t num_connections,
	int *np, fd_set *readfds, fd_set *writefds, fd_set *exceptfds,
	struct timeval *tv)
{
    size_t count = 0;
    size_t i;
    time_t now = time(NULL);
    int minexpiry = 360000;

    if (cs == NULL) return 0;
    for(i=0;i<num_connections;++i) {
	Connection c = cs[i];
	int needs_read = 0;
	int needs_write = 0;
	int fd;
	
	if (c == NULL) continue;

	fd = c->fd;
	if (fd >= 0) {
	    if (c->connectionstate == CONNECTED_SUPERNODE_WAITING ||
	            c->connectionstate == CONNECTED_SUPERNODE_SLURPING ||
	            c->connectionstate == CONNECTED_SUPERNODE_DECRYPTING ||
		    c->connectionstate == CONNECTED_SUPERNODE_CONNECTED ||
		    c->connectionstate == CONNECTED_NODE_CONNECTED ||
		    c->connectionstate == CONNECTED_CLIENT_READING ||
		    c->connectionstate == CONNECTED_PEER_READING ||
		    c->connectionstate == CONNECTED_DOWNLOAD_READING) {
		needs_read = 1;
		FD_SET(fd, readfds);
	    }
	    if (c->writebufwritten < c->writebufsize ||
		    c->connectionstate == CONNECTED_NODE_CONNECTING ||
		    c->connectionstate == CONNECTED_SUPERNODE_CONNECTING ||
		    c->connectionstate == CONNECTED_DOWNLOAD_CONNECTING ||
		    c->connectionstate == CONNECTED_CLIENT_FLUSHING ||
		    c->connectionstate == CONNECTED_PEER_FLUSHING ||
		    c->connectionstate == CONNECTED_PEER_STREAMING) {
		needs_write = 1;
		FD_SET(fd, writefds);
	    }
	    if (needs_read || needs_write) {
		if ((fd+1) > *np) {
		    *np = fd+1;
		}
		++count;
	    }
	} else if (c->connectionstate == CONNECTED_DOWNLOAD_WAITING) {
	    ++count;
	}
	if (c->expiry - now < minexpiry) {
	    minexpiry = c->expiry - now;
	}
    }
    if (minexpiry < 0) {
	minexpiry = 0;
    } else {
	++minexpiry;
    }
    if (tv->tv_sec > minexpiry) {
	tv->tv_sec = minexpiry;
	tv->tv_usec = 0;
    }

    return count;
}

/**
 * Slurp encrypted data from a connection, and just store it until we
 * can try to decrypt it later.
 */
static int slurpFromConnection(Connection c, int which,
	ConnectionChangedHandler cch, void *cch_context)
{
    int res;
    byte buf[1600];

    if (c == NULL ||
	    (c->connectionstate != CONNECTED_SUPERNODE_WAITING &&
	     c->connectionstate != CONNECTED_SUPERNODE_SLURPING)) {
	return -1;
    }
    res = read(c->fd, buf, sizeof(buf));
    if (res <= 0) return res;

    if (c->slurpread + res > c->slurpsize) {
	size_t newsize = c->slurpread + res + 4000;
	byte *newslurp = realloc(c->slurpbuf, newsize);
	if (newslurp == NULL) {
	    return -1;
	}
	c->slurpbuf = newslurp;
	c->slurpsize = newsize;
    }
    memmove(c->slurpbuf + c->slurpread, buf, res);
    c->slurpread += res;

    if (c->connectionstate == CONNECTED_SUPERNODE_WAITING) {
	ConnectionState oldstate = c->connectionstate;
	c->connectionstate = CONNECTED_SUPERNODE_SLURPING;
	cch(which, oldstate, c->connectionstate, cch_context);
    }
    return res;
}

/**
 * Read encrypted data whose plaintext we expect to understand, and try
 * to determine the cipher state.
 */
static int decryptFromConnection(Connection c, int which,
	ConnectionChangedHandler cch, void *cch_context,
	AddressHandler ah, void *ah_context,
	PacketHandler ph, void *ph_context)
{
    ConnectionState oldstate;
    int res;

    if (c == NULL || c->connectionstate != CONNECTED_SUPERNODE_DECRYPTING) {
	return -1;
    }
    res = read(c->fd, c->decryptbuf + c->decryptread,
	    NUM_PINGS - c->decryptread);
    if (res <= 0) return res;

    c->decryptread += res;
    if (c->decryptread < NUM_PINGS) {
	return res;
    }

    /* We've got what we want to decrypt.  If we fail after this point,
     * be sure to connect again to retry. */
    c->reconnect = 1;

    /* Try to recover the cipher state. */
    res = decrypt_pongs(c->decryptbuf, NUM_PINGS, &(c->rcs));

    if (res <= 0) {
	return res;
    }

    /* Now try to go back and decrypt the slurped data. */
    res = decrypt_old_data(c->slurpbuf, c->slurpread, c->decryptbuf, NUM_PINGS,
	    &(c->rcs));

    if (res <= 0) {
	return res;
    }

    /* See if we can find any addresses lying around */
    if (ah && c->slurpread > 8) {
	int i;
	for(i=4;i<c->slurpread-3;++i) {
	    if (c->slurpbuf[i] == 0x04 && c->slurpbuf[i+1] == 0xbe) {
		struct sockaddr_in sin;
		sin.sin_family = AF_INET;
		sin.sin_port = htons(0x04be);
		sin.sin_addr.s_addr = htonl(
			(c->slurpbuf[i-4]<<24) |
			(c->slurpbuf[i-3]<<16) |
			(c->slurpbuf[i-2]<<8) |
			(c->slurpbuf[i-1]) );
		ah(which, (struct sockaddr *)&sin, sizeof(sin), ah_context);
	    }
	}
    }

    /* See if we can find any stats lying around */
    if (c->slurpread > 65) {
	int i;
	for(i=0;i<c->slurpread-65;++i) {
	    if (!memcmp(c->slurpbuf+i, "\x4b\x09\x00\x00\x3c", 5) ||
		    !memcmp(c->slurpbuf+i, "\x4b\x00\x00\x09\x3c", 5) ||
		    !memcmp(c->slurpbuf+i, "\x4b\x00\x3c\x00\x09", 5)) {
		handle_packet(c, which, 9, 60, c->slurpbuf+i+5,
			ph, ph_context);
	    }
	}
    }

    /* Free the slurped memory. */
    if (c->slurpbuf) {
	free(c->slurpbuf);
	c->slurpbuf = NULL;
	c->slurpsize = 0;
	c->slurpread = 0;
    }

    /* We now have a CipherState. */
    oldstate = c->connectionstate;
    c->connectionstate = CONNECTED_SUPERNODE_CONNECTED;
    cch(which, oldstate, c->connectionstate, cch_context);

    /* Send our network name. */
    writePacket(c, 0x1d, c->networkname, strlen(c->networkname));

    return res;
}

/**
 * Write a X-giFT-SupernodeIP: header.
 */
static int headerwriter(int which, struct sockaddr *addr, socklen_t addrlen,
	void *context)
{
    Connection peer = context;
    struct sockaddr_in *sin;
    char colonport[10];

    if (addr == NULL) return -1;
    if (addr->sa_family != AF_INET) return -1;
    sin = (struct sockaddr_in *)addr;
    writeRawString(peer, "\r\nX-giFT-SupernodeIP: ");
    writeRawString(peer, inet_ntoa(sin->sin_addr));
    sprintf(colonport, ":%u", ntohs(sin->sin_port));
    writeRawString(peer, colonport);
    return 0;
}

/**
 * Show some standard HTTP headers.
 */
static void show_headers(Connection c, PeerServerContext *psc)
{
    struct in_addr myaddr;

    writeRawString(c, "Accept-Ranges: bytes\r\n"
	    "Server: giFT " VERSION " " __DATE__ " " __TIME__ "\r\n"
	    "Connection: close\r\n");
    
    writeRawString(c, "X-giFT-Username: ");
    writeRawString(c, psc->username);
    writeRawString(c, "\r\nX-giFT-Networkname: ");
    writeRawString(c, psc->networkname);

    myaddr = psc->server_addr.sin_addr;
    if (myaddr.s_addr == INADDR_ANY) {
	struct sockaddr_in myname;
	socklen_t namelen = sizeof(myname);
	if (getsockname(c->fd, (struct sockaddr *)&myname, &namelen) == 0
		&& myname.sin_family == AF_INET) {
	    myaddr = myname.sin_addr;
	}
    }
    if (myaddr.s_addr != INADDR_ANY) {
	char colonport[10];
	writeRawString(c, "\r\nX-giFT-IP: ");
	writeRawString(c, inet_ntoa(myaddr));
	sprintf(colonport, ":%u", ntohs(psc->server_addr.sin_port));
	writeRawString(c, colonport);
    }
    getConnecteds(psc->cs, psc->numcs, headerwriter, c);
    writeRawString(c, "\r\n");
}

/**
 * Read an HTTP request from a peer connection.
 */
static int readFromPeer(Connection c, int which, PeerServerContext *psc,
	ConnectionChangedHandler cch, void *cch_context)
{
    int res;
    byte *range, *end, *uri, *s, *d;
    ConnectionState oldstate, newstate = CONNECTED_NULL;
    const char *notimperror = "HTTP/1.0 501 Not Implemented\r\n";
    const char *notfounderror = "HTTP/1.0 404 Not Found\r\n";
    const char *rangeerror = "HTTP/1.0 416 Requested Range Not Satisfiable\r\n";
    int contentlength = -1;
    int rangestart = -1, rangeend = -1;
    int mode = -1;
    char rbuf[40];
    const char *contenttype = NULL;
    void *handle = NULL;
    ShareHandle sharehandle;

    if (c == NULL || c->fd < 0 || psc == NULL) return -1;
    sharehandle = psc->sh;

    res = read(c->fd, c->packetbuf + c->packetread,
	    c->packetsize - c->packetread);
    if (res <= 0) return res;
    c->packetread += res;
    c->packetbuf[c->packetread] = '\0';

    /* Do we have enough to decide what to do? */
    if (c->packetread < 4) return res;

    if (c->packetbuf[0] == 0x80) {
	/* Someone thinks we're a supernode.  Dump them. */
	return -1;
    }

    /* See if we've gotten the whole request yet. */
    end = strstr(c->packetbuf, "\r\n\r\n");
    if (!end) {
	/* Not yet. */
	return res;
    }

    /* Is the request of a type we recognize? */
    if (strncmp(c->packetbuf, "GET ", 4)) {
	writeRawString(c, notimperror);
	show_headers(c, psc);
	writeRawString(c, "\r\n");

	oldstate = c->connectionstate;
	c->connectionstate = CONNECTED_PEER_FLUSHING;
	cch(which, oldstate, c->connectionstate, cch_context);
	return res;
    }

    /* See if there's a Range: header. */
    range = strstr(c->packetbuf, "\nRange:");
    if (range) {
	range += 7;
	while(*range && *range == ' ') ++range;
	if (!strncmp(range, "bytes=", 6)) {
	    range += 6;
	    if (range[0] >= '0' && range[0] <= '9') {
		sscanf(range, "%u-%u", &rangestart, &rangeend);
	    } else if (range[0] == '-') {
		sscanf(range+1, "%u", &rangeend);
	    }
	}
    }

    /* Find the URI. */
    end = strchr(c->packetbuf, '\r');
    if (end) *end = '\0';
    uri = c->packetbuf+4;
    end = strchr(uri, ' ');
    if (end) *end = '\0';

    /* Unescape the URI. */
    for(s=uri, d=uri; *s; ++s) {
	byte c = *s;
	static const char hex[] = "0123456789abcdefABCDEF";
	if (c == '+') {
	    *(d++) = ' ';
	} else if (c == '%' && s[1] && s[2] &&
		strchr(hex, s[1]) && strchr(hex, s[2])) {
	    int h;
	    int res = sscanf(s+1, "%2x", &h);
	    if (res == 1) {
		*(d++) = h;
		s += 2;
	    } else {
		*(d++) = '%';
	    }
	} else {
	    *(d++) = c;
	}
    }
    *d = '\0';

    /* Figure out how big the total output will be. */
    if (!strcmp(uri, "/")) {
	contentlength = shareGetTotalLength(sharehandle, 0);
	contenttype = "text/html";
	mode = 0;
    } else if (!strcmp(uri, "/.files")) {
	contentlength = shareGetTotalLength(sharehandle, 1);
	contenttype = "application/octet-stream";
	mode = 1;
    } else if (uri[0] == '/') {
	/* Skip the checksum and the next / */
	++uri;
	while (uri[0] >= '0' && uri[0] <= '9') ++uri;
	if (uri[0] == '/') {
	    ++uri;
	    handle = shareGetFileLengthAndType(sharehandle, uri,
		    &contentlength, &contenttype);
	    mode = 2;
	}
    }

    if (contentlength < 0 || mode < 0) {
	/* We don't know how to serve this URI. */
	writeRawString(c, notfounderror);
	show_headers(c, psc);
	writeRawString(c, "\r\n");
	oldstate = c->connectionstate;
	c->connectionstate = CONNECTED_PEER_FLUSHING;
	cch(which, oldstate, c->connectionstate, cch_context);
	return res;
    }

    /* Work out the start and end locations. */
    if (rangestart == -1 && rangeend >= 0) {
	rangestart = contentlength - rangeend;
	rangeend = contentlength-1;
    } else if (rangestart == -1) {
	rangestart = 0;
	rangeend = contentlength-1;
    } else if (rangeend == -1) {
	rangeend = contentlength-1;
    }

    /* Did it make any sense? */
    if (rangestart < 0 || (contentlength > 0 && rangestart > rangeend) ||
	    rangeend >= contentlength) {
	/* Invalid range. */
	writeRawString(c, rangeerror);
	show_headers(c, psc);
	writeRawString(c, "\r\n");
	oldstate = c->connectionstate;
	c->connectionstate = CONNECTED_PEER_FLUSHING;
	cch(which, oldstate, c->connectionstate, cch_context);
	return res;
    }
    
    /* Send the headers. */
    oldstate = c->connectionstate;
    c->connectionstate = CONNECTED_PEER_WRITING;
    cch(which, oldstate, c->connectionstate, cch_context);
    if (rangestart > 0 || rangeend < contentlength-1) {
	/* This is a partial send. */
	writeRawString(c, "HTTP/1.0 206 Partial Content\r\n"
		"Content-Range: bytes ");
	sprintf(rbuf, "%u-%u/%u", rangestart, rangeend, contentlength);
	writeRawString(c, rbuf);
	writeRawString(c, "\r\nContent-Length: ");
	sprintf(rbuf, "%u", rangeend-rangestart+1);
	writeRawString(c, rbuf);
	writeRawString(c, "\r\n");
    } else {
	writeRawString(c, "HTTP/1.0 200 OK\r\n");
	writeRawString(c, "Content-Length: ");
	sprintf(rbuf, "%u", rangeend-rangestart+1);
	writeRawString(c, rbuf);
	writeRawString(c, "\r\n");
    }
	
    show_headers(c, psc);

    writeRawString(c, "Content-Type: ");
    writeRawString(c, contenttype);
    writeRawString(c, "\r\n\r\n");

    /* Now start sending the data. */
    switch(mode) {
	case 0:
	case 1:
	    c->outputskip = rangestart;
	    c->outputmax = rangeend - rangestart + 1;
	    newstate = shareWriteFileList(sharehandle, c, mode);
	    break;
	case 2:
	    newstate = shareWriteFile(sharehandle, c, handle, rangestart,
		    rangeend-rangestart+1);
	    break;
    }

    if (newstate != c->connectionstate) {
	oldstate = c->connectionstate;
	c->connectionstate = newstate;
	cch(which, oldstate, c->connectionstate, cch_context);
    }

    return res;
}

/**
 * Read data from a peer we're trying to get a file from.
 */
int readFromDownload(Connection c, int which, DownloadHandler dh,
	void *dh_context, ConnectionChangedHandler cch, void *cch_context)
{
    int res;
    byte *end, *succode;
    byte *datastart;
    size_t datalen;

    /* Try to read the whole header. */
    if (c->packetread == c->packetsize) {
	size_t newsize = c->packetsize + 16000;
	byte *newbuf = realloc(c->packetbuf, newsize+1);
	if (!newbuf) return -1;
	c->packetbuf = newbuf;
	c->packetsize = newsize;
    }
    res = read(c->fd, c->packetbuf + c->packetread,
	    c->packetsize - c->packetread);
    if (res < 0) return res;
    if (res == 0) {
	if (c->dlgettingdata) {
	    /* We finished the download; requeue to get more work. */
	    c->dlwaittime = 1;
	    dl_waiting(c, which, cch, cch_context);
	    return 1;
	}
	return 0;
    }
    c->packetread += res;
    c->packetbuf[c->packetread] = '\0';

    if (c->dlgettingdata == 0) {
	/* See if we've received the whole response header yet. */
	end = strstr(c->packetbuf, "\r\n\r\n");
	if (!end) {
	    /* The server incorrectly uses this to terminate 503 messages. */
	    end = strstr(c->packetbuf, "\r\n\n");
	    if (!end) {
		/* Not yet. */
		return res;
	    }
	}

	/* Put a NUL after the first \r\n of the end of the headers.
	 * The data, if any, starts at end+4. */
	end[2] = '\0';

	/* Do the headers indicate success? */
	succode = c->packetbuf;
	while (*succode && *succode != ' ') ++succode;
	if (*succode == ' ') ++succode;
	if (!strncmp(succode, "503", 3)) {
	    byte *retryafter = strstr(c->packetbuf, "\nRetry-After:");

	    /* This site is busy.  Try again later. */
	    if (retryafter) {
		c->dlwaittime = atoi(retryafter + 13);
	    } else {
		c->dlwaittime = 3;
	    }
	    dl_waiting(c, which, cch, cch_context);
	    return res;
	} else if (succode[0] != '2') {
	    /* Failed.  Don't try again. */
	    return 0;
	}

	c->dlgettingdata = 1;

	/* Deal with any actual data we received. */
	if (c->packetbuf + c->packetread > end+4) {
	    datastart = end+4;
	    datalen = c->packetbuf + c->packetread - (end+4);
	} else {
	    datastart = c->packetbuf + c->packetread;
	    datalen = 0;
	}
    } else {
	datastart = c->packetbuf;
	datalen = c->packetread;
    }

    if (dh && datalen > 0) {
	int dhres = dh(which, c->dlcontext, c->dlid, NULL, &(c->dlpos),
		&datalen, datastart, dh_context);
	if (dhres < 0) {
	    /* Disconnect this one, and fetch more data right
	     * away. */
	    c->dlwaittime = 1;
	    dl_waiting(c, which, cch, cch_context);
	} else {
	    c->dlpos += datalen;
	    c->packetread = 0;
	}
    }

    return res;
}

/**
 * Handle the results from a select() call.  If a packet becomes fully
 * available, the supplied PacketHandler will be called.  If a query
 * from a client becomes available, the QueryHandler will be called.  If
 * the connection changes state, the ConnectionChangedHandler will be
 * called.
 */
void selectHandleConnections(Connection *cs, size_t num_connections,
	int select_retval, fd_set *readfds, fd_set *writefds,
	fd_set *execptfds, PacketHandler ph, void *ph_context,
	QueryHandler qh, void *qh_context, ConnectionChangedHandler cch,
	void *cch_context, AddressHandler ah, void *ah_context,
	PeerServerContext *psc, DownloadHandler dh, void *dh_context)
{
    size_t i;
    int res;
    time_t now = time(NULL);

    if (cs == NULL) return;

    if (select_retval > 0) {
	for(i=0;i<num_connections;++i) {
	    Connection c = cs[i];
	    int fd;
	    int was_closed = 0;
	    int handled = 0;
	    
	    if (c == NULL || c->fd < 0) continue;

	    fd = c->fd;
	    if (FD_ISSET(fd, writefds)) {
		switch(c->connectionstate) {
		    case CONNECTED_NODE_CONNECTED:
		    case CONNECTED_SUPERNODE_WAITING:
		    case CONNECTED_SUPERNODE_SLURPING:
		    case CONNECTED_SUPERNODE_DECRYPTING:
		    case CONNECTED_SUPERNODE_CONNECTED:
		    case CONNECTED_CLIENT_WRITING:
		    case CONNECTED_PEER_WRITING:
		    case CONNECTED_DOWNLOAD_READING:
			res = writeToConnection(c);
			break;
		    case CONNECTED_CLIENT_FLUSHING:
		    case CONNECTED_PEER_FLUSHING:
			res = writeToConnection(c);
			if (res > 0 && c->writebufsize == 0) {
			    /* We finished writing.  We're done now. */
			    was_closed = 1;
			}
			break;
		    case CONNECTED_PEER_STREAMING:
			res = streamToConnection(c);
			break;
		    case CONNECTED_SUPERNODE_CONNECTING:
		    case CONNECTED_NODE_CONNECTING:
		    case CONNECTED_DOWNLOAD_CONNECTING:
			res = connectConnection(c, i, dh, dh_context,
				cch, cch_context);
			break;
		    default:
			res = -1;
			break;
		}
		if (res <= 0) was_closed = 1;
		handled = 1;
	    }
	    if (FD_ISSET(fd, readfds)) {
		switch(c->connectionstate) {
		    case CONNECTED_SUPERNODE_WAITING:
		    case CONNECTED_SUPERNODE_SLURPING:
			res = slurpFromConnection(c, i, cch, cch_context);
			break;
		    case CONNECTED_SUPERNODE_DECRYPTING:
			res = decryptFromConnection(c, i, cch, cch_context,
				ah, ah_context, ph, ph_context);
			break;
		    case CONNECTED_SUPERNODE_CONNECTED:
			res = readFromConnection(c, i, ph, ph_context,
				cch, cch_context);
			break;
		    case CONNECTED_NODE_CONNECTED:
			res = readNodeResponse(c, i, cch, cch_context);
			break;
		    case CONNECTED_CLIENT_READING:
			res = readFromClient(c, i, qh, qh_context,
				cch, cch_context);
			break;
		    case CONNECTED_PEER_READING:
			res = readFromPeer(c, i, psc, cch, cch_context);
			break;
		    case CONNECTED_DOWNLOAD_READING:
			res = readFromDownload(c, i, dh, dh_context,
				cch, cch_context);
			if (c->connectionstate != CONNECTED_DOWNLOAD_READING
				|| res <= 0) {
			    /* We're no longer requesting the chunk we
			     * were a moment ago. */
			    if (dh) dh(i, c->dlcontext, c->dlid, NULL,
				    NULL, NULL, c->packetbuf, dh_context);
			}
			break;
		    default:
			res = -1;
			break;
		}
		if (res <= 0) was_closed = 1;
		handled = 1;
	    }
	    if (was_closed) {
		ConnectionState oldstate = c->connectionstate;
		c->connectionstate = CONNECTED_CLOSED;
		cch(i, oldstate, c->connectionstate, cch_context);
	    }
	    if (handled) {
		/* This Connection was active just now, so update its
		 * expiry time according to its state. */
		switch(c->connectionstate) {
		    case CONNECTED_NULL:
		    case CONNECTED_CLOSED:
			c->expiry = now;
			break;
		    case CONNECTED_NODE_CONNECTING:
		    case CONNECTED_SUPERNODE_CONNECTING:
			c->expiry = now + TIMEOUT_CONNECTING;
			break;
		    case CONNECTED_NODE_CONNECTED:
			c->expiry = now + TIMEOUT_NODE_CONNECTED;
			break;
		    case CONNECTED_SUPERNODE_WAITING:
			c->expiry = now + TIMEOUT_SUPERNODE_WAITING;
			break;
		    case CONNECTED_SUPERNODE_SLURPING:
			c->expiry = now + TIMEOUT_SUPERNODE_SLURPING;
			break;
		    case CONNECTED_SUPERNODE_DECRYPTING:
			c->expiry = now + TIMEOUT_SUPERNODE_DECRYPTING;
			break;
		    case CONNECTED_SUPERNODE_CONNECTED:
			c->expiry = now + TIMEOUT_SUPERNODE_CONNECTED;
			break;
		    case CONNECTED_CLIENT_READING:
			c->expiry = now + TIMEOUT_CLIENT_READING;
			break;
		    case CONNECTED_CLIENT_WRITING:
		    case CONNECTED_CLIENT_FLUSHING:
			c->expiry = now + TIMEOUT_CLIENT_WRITING;
			break;
		    case CONNECTED_PEER_READING:
			c->expiry = now + TIMEOUT_PEER_READING;
			break;
		    case CONNECTED_PEER_WRITING:
		    case CONNECTED_PEER_FLUSHING:
			c->expiry = now + TIMEOUT_PEER_WRITING;
			break;
		    case CONNECTED_PEER_STREAMING:
			c->expiry = now + TIMEOUT_PEER_STREAMING;
			break;
		    case CONNECTED_DOWNLOAD_CONNECTING:
			c->expiry = now + TIMEOUT_DOWNLOAD_CONNECTING;
			break;
		    case CONNECTED_DOWNLOAD_READING:
			c->expiry = now + TIMEOUT_DOWNLOAD_READING;
			break;
		    case CONNECTED_DOWNLOAD_WAITING:
			c->expiry = now + c->dlwaittime;
			c->dlwaittime = 0;
			break;
		    case CONNECTED_DEAD:
			c->expiry = now + TIMEOUT_DEAD;
			break;
		}
	    }
	}
    }

    /* Now see if anyone's expired */
    for(i=0;i<num_connections;++i) {
	if (cs[i] && cs[i]->expiry <= now) {
	    ConnectionState oldstate = cs[i]->connectionstate;
	    if (oldstate == CONNECTED_SUPERNODE_SLURPING) {
		int j;
		cs[i]->connectionstate = CONNECTED_SUPERNODE_DECRYPTING;
		cch(i, oldstate, cs[i]->connectionstate, cch_context);
		for(j=0;j<NUM_PINGS;++j) {
		    writeRawData(cs[i], "\x50", 1, 1);
		}
	    } else if (oldstate == CONNECTED_DOWNLOAD_WAITING) {
		/* Save some of the info, and reconnect. */
		DownloadContext dc = cs[i]->dlcontext;
		size_t dlid = cs[i]->dlid;
		size_t addrlen = cs[i]->addrlen;
		int dlretries = cs[i]->dlretries;
		struct sockaddr *addr = malloc(addrlen);
		int reconnect = 0;

		if (addr) {
		    memmove(addr, cs[i]->addr, addrlen);
		    reconnect = 1;
		}

		closeConnection(cs+i);
		cch(i, oldstate, CONNECTED_NULL, cch_context);
		if (reconnect) {
		    cs[i] = newDownloadConnection(dc, dlid, addr, addrlen);
		    if (cs[i]) {
			cs[i]->dlretries = dlretries;
		    }
		}

		if (addr) free(addr);
	    } else if (oldstate == CONNECTED_CLOSED && cs[i]->reconnect == 1) {
		/* Save some of the info, and reconnect right away. */
		char *username = strdup(cs[i]->username);
		char *networkname = strdup(cs[i]->networkname);
		size_t addrlen = cs[i]->addrlen;
		struct sockaddr_in server_addr = cs[i]->server_addr;
		struct sockaddr *addr = malloc(addrlen);
		int reconnect = 0;

		if (username && networkname && addr) {
		    memmove(addr, cs[i]->addr, addrlen);
		    reconnect = 1;
		}

		closeConnection(cs+i);
		cch(i, oldstate, CONNECTED_NULL, cch_context);
		if (reconnect) {
		    cs[i] = startConnection(addr, addrlen, username,
			    networkname, &server_addr);
		}

		if (username) free(username);
		if (networkname) free(networkname);
		if (addr) free(addr);
	    } else {
		/* Goodbye! */
		closeConnection(cs+i);
		cch(i, oldstate, CONNECTED_NULL, cch_context);
	    }
	}
    }
}

/**
 * Add the number of each ConnectionState in the given range of
 * Connections to the running stats.
 */
void accumulateStats(Connection *cs, size_t nconn, int stats[])
{
    int i;
    Connection c;

    for(i=0;i<nconn;++i) {
	c = cs[i];
	if (c == NULL) {
	    ++stats[CONNECTED_NULL];
	} else {
	    ++stats[c->connectionstate];
	}
    }
}

/**
 * Compare two addresses.  Returns 0 if they match, non-zero otherwise.
 */
static int addrcmp(struct sockaddr *s1, socklen_t s1l,
	struct sockaddr *s2, socklen_t s2l)
{
    if (s1l != s2l) return s1l-s2l;
    if (s1->sa_family == AF_INET && s2->sa_family == AF_INET) {
	struct sockaddr_in *s1in = (struct sockaddr_in *)s1;
	struct sockaddr_in *s2in = (struct sockaddr_in *)s2;
	if (s1in->sin_addr.s_addr != s2in->sin_addr.s_addr) {
	    return s1in->sin_addr.s_addr - s2in->sin_addr.s_addr;
	}
	if (s1in->sin_port != s2in->sin_port) {
	    return s1in->sin_port - s2in->sin_port;
	}
	return 0;
    }
    return memcmp(s1, s2, s1l);
}

/**
 * Compare the address for the given Connection to the given one.
 */
int cmpConnectionAddr(Connection c, struct sockaddr *addr,
	socklen_t addrlen)
{
    if (c == NULL) return -1;

    return addrcmp(c->addr, c->addrlen, addr, addrlen);
}

/**
 * Add the given address to the connection list.  Return 0 if we don't
 * want to add it, 1 if we added it, and -1 if we'd like to add it, but
 * we're full.
 */
int addToConnectionList(Connection *cs, size_t nconn, size_t maxconn,
	struct sockaddr *addr, socklen_t addrlen, char *username,
	char *networkname, struct sockaddr_in *server_addr)
{
    int i, nullslot = -1, oldestdead = -1, numdead = 0, numconn = 0;

    /* See if we've already got this one.  At the same time, find an
     * empty slot.  Also find the oldest dead slot and the number of
     * dead slots and the number of connected slots. */
    for(i=0;i<nconn;++i) {
	if (cs[i] == NULL) {
	    if (nullslot == -1) {
		nullslot = i;
	    }
	} else {
	    if (!addrcmp(cs[i]->addr, cs[i]->addrlen, addr, addrlen)) {
		/* We have this one already */
		return 0;
	    }
	    if (cs[i]->connectionstate == CONNECTED_DEAD) {
		if (oldestdead < 0 || cs[i]->expiry < cs[oldestdead]->expiry) {
		    oldestdead = i;
		}
		++numdead;
	    }
	    if (cs[i]->connectionstate == CONNECTED_SUPERNODE_CONNECTED) {
		++numconn;
	    }
	}
    }
    
    /* If we have enough connections already, just punt this one back to
     * the waitlist. */
    if (numconn >= maxconn) {
	return -1;
    }

    if (nullslot == -1) {
	/* No room!  See if we can evict a dead slot. */
	if (numdead > nconn/2) {
	    closeConnection(cs+oldestdead);
	    nullslot = oldestdead;
	} else {
	    return -1;
	}
    }

    cs[nullslot] = startConnection(addr, addrlen, username, networkname,
	    server_addr);
    return (cs[nullslot] == NULL ? 0 : 1);
}

/**
 * Get the list of fully-connected nodes.  Each one will be passed in
 * turn to the AddressHandler.
 */
void getConnecteds(Connection *cs, size_t nconn, AddressHandler ah,
	void *ah_context)
{
    size_t i,j;
    size_t offset;

    if (nconn == 0) return;
    offset = rand() % nconn;

    for(j=0;j<nconn;++j) {
	i = (j+offset) % nconn;
	if (cs[i] && cs[i]->connectionstate == CONNECTED_SUPERNODE_CONNECTED) {
	    ah(i, cs[i]->addr, cs[i]->addrlen, ah_context);
	}
    }
}

/**
 * Create a new Connection when we get contacted by a client (e.g. the
 * CGI script).
 */
static Connection newClientOrPeerConnection(int fd, struct sockaddr *addr,
	socklen_t addrlen, ConnectionState state, int timeout)
{
    time_t now = time(NULL);
    Connection retval;
    byte *buf;
    struct sockaddr *newaddr;
    unsigned long flags;

    /* Make the fd non-blocking */
    fcntl(fd, F_GETFL, &flags);
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
	return NULL;
    }

    newaddr = malloc(addrlen);
    if (newaddr == NULL) {
	return NULL;
    }
    memmove(newaddr, addr, addrlen);

    buf = malloc(16001);
    if (buf == NULL) {
	free(newaddr);
	return NULL;
    }
    buf[16000] = '\0';

    retval = malloc(sizeof(struct s_Connection));
    if (retval == NULL) {
	free(newaddr);
	free(buf);
	return NULL;
    }

    initConnection(retval, state);
    retval->fd = fd;
    retval->expiry = now + timeout;
    retval->addr = newaddr;
    retval->addrlen = addrlen;
    retval->packetbuf = buf;
    retval->packetsize = 16000;
    retval->packetread = 0;

    return retval;
}

/**
 * Add a new client to a Connection list.
 */
int addToClientsList(Connection *cs, size_t nconn, struct sockaddr *addr,
	socklen_t addrlen, int fd)
{
    size_t i;

    /* Find an empty slot */
    for(i=0;i<nconn;++i) {
	if (cs[i] == NULL) {
	    cs[i] = newClientOrPeerConnection(fd, addr, addrlen,
			CONNECTED_CLIENT_READING, TIMEOUT_CLIENT_READING);
	    if (cs[i] == NULL) {
		return -1;
	    }
	    return 0;
	}
    }
    return -1;
}

/**
 * Set the query id for a client Connection.  Also increment a reference
 * count to keep track of the number of times this has been called for
 * this Connection.
 */
void setClientID(Connection c, unsigned short qid)
{
    if (c == NULL) return;

    c->qid = qid;
    c->refcount++;
}

/**
 * Indicate that we're done queueing data for the client with the given
 * id.  Decrement the reference count, and when it reaches 0, really
 * flush the connection.
 */
void flushClientConnection(Connection *clients, size_t nconn,
	unsigned short qid, ConnectionChangedHandler cch, void *cch_context)
{
    size_t i;

    if (clients == NULL) return;

    for(i=0;i<nconn;++i) {
	Connection c = clients[i];
	if (c && c->qid == qid) {
	    if (c->refcount > 0) c->refcount--;
	    if (c->refcount == 0) {
		ConnectionState oldstate = c->connectionstate;
		c->connectionstate = CONNECTED_CLIENT_FLUSHING;
		cch(i, oldstate, c->connectionstate, cch_context);
	    }
	}
    }
}

typedef struct {
    Connection c;
} CWContext;

/**
 * This ClientWriter will be passed back to the main program to be
 * called when data needs to be enqueued for writing to a particular
 * client Connection.
 */
static int clientWrite(const byte *buf, size_t size, void *context)
{
    CWContext *cwc = context;
    if (!cwc) return -1;
    if (size > 0 && buf == NULL) return -1;
    if (size == 0) return 0;
    writeRawData(cwc->c, buf, size, 0);
    return size;
}

/**
 * Get a ClientWriter associated with the given query id.
 */
int getClientWriterByQid(Connection *clients, size_t nconn,
	unsigned short qid, ClientWriter *cwp, void **cw_contextp)
{
    size_t i;

    if (clients == NULL) return -1;

    for(i=0;i<nconn;++i) {
	Connection c = clients[i];
	if (c && c->qid == qid) {
	    return getClientWriterByConnection(c, cwp, cw_contextp);
	}
    }
    return -1;
}

/**
 * Get a ClientWriter for a given client Connection.
 */
int getClientWriterByConnection(Connection client, ClientWriter *cwp,
	void **cw_contextp)
{
    CWContext *cwc;
    
    if (client == NULL) return -1;

    cwc = malloc(sizeof(CWContext));
    if (!cwc) return -1;

    cwc->c = client;
    if (cwp) *cwp = clientWrite;
    if (cw_contextp) {
	*cw_contextp = cwc;
    } else {
	free(cwc);
    }
    return 0;
}

/**
 * Free a ClientWriter context.
 */
void freeCWContext(void **cw_contextp)
{
    CWContext *cwc;

    if (cw_contextp == NULL) return;
    cwc = *cw_contextp;
    if (cwc) free(cwc);
    *cw_contextp = NULL;
}

/**
 * Add a new peer to a Connection list.
 */
int addToPeersList(Connection *cs, size_t nconn, struct sockaddr *addr,
	socklen_t addrlen, int fd)
{
    size_t i;

    /* Find an empty slot */
    for(i=0;i<nconn;++i) {
	if (cs[i] == NULL) {
	    cs[i] = newClientOrPeerConnection(fd, addr, addrlen,
			CONNECTED_PEER_READING, TIMEOUT_PEER_READING);
	    if (cs[i] == NULL) {
		return -1;
	    }
	    return 0;
	}
    }
    return -1;
}

/**
 * Initiate a connection with a peer in order to download data from it.
 */
Connection newDownloadConnection(DownloadContext dc, size_t dlid,
	struct sockaddr *nodeaddr, socklen_t addrlen)
{
    int s;
    Connection retval = NULL;
    struct sockaddr *copyaddr = NULL;
    time_t now = time(NULL);

    if (dc == NULL || nodeaddr == NULL || addrlen == 0)
	return NULL;

    /* Copy the dest address */
    copyaddr = malloc(addrlen);
    if (copyaddr == NULL) {
	return NULL;
    }
    memmove(copyaddr, nodeaddr, addrlen);

    /* Make the socket and start the connection */
    s = makeNBSocket(nodeaddr, addrlen);
    if (s < 0) {
	free(copyaddr);
	return NULL;
    }

    /* Now make the Connection handle and return it */
    retval = malloc(sizeof(struct s_Connection));
    if (retval == NULL) {
	close(s);
	free(copyaddr);
	return NULL;
    }

    initConnection(retval, CONNECTED_DOWNLOAD_CONNECTING);
    retval->fd = s;
    retval->expiry = now + TIMEOUT_DOWNLOAD_CONNECTING;
    retval->addr = copyaddr;
    retval->addrlen = addrlen;
    retval->dlid = dlid;
    retval->dlcontext = dc;

    return retval;
}
