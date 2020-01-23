#ifndef __giFTPACKET_H__
#define __giFTPACKET_H__

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "giFT.h"

/**
 * The Connection data type refers to one of the following:
 * - An outgoing TCP connection to a node or supernode
 * - An incoming TCP connection from a client (on port 1213)
 * - An incoming TCP connection from a peer (not implemented yet)
 */
typedef struct s_Connection *Connection;

/**
 * The states a Connection can be in.  Briefly:
 * CONNECTED_NULL:
 *     The Connection pointer is the NULL pointer.
 * CONNECTED_NODE_CONNECTING:
 *     We are trying to establish an outgoing TCP connection to a node
 *     in order to ask it who its supernode is.
 * CONNECTED_NODE_CONNECTED:
 *     The above TCP session has completed, and we have sent a
 *     "GET / HTTP/1.0" request to the node.  We expect a response whose
 *     headers will indicate the indentity of this node's supernode.
 * CONNECTED_SUPERNODE_CONNECTING:
 *     We have closed the connection to the above node, and are trying
 *     to establish one to the supernode.
 * CONNECTED_SUPERNODE_WAITING:
 *     The TCP connection to the supernode has completed, and we have
 *     sent the username message.  We are waiting for the first packet
 *     to come back.
 * CONNECTED_SUPERNODE_SLURPING:
 *     We have received the first packet, and we are slurping in as much
 *     data as we can in an attempt to find known plaintext to recover
 *     the cipher state.
 * CONNECTED_SUPERNODE_DECRYPTING:
 *     We are receiving packets with known plaintext, which will enable
 *     us to recover the cipher state, and go on.
 * CONNECTED_SUPERNODE_CONNECTED:
 *     We have determined the cipher state of the supernode, and can now
 *     communicate bidirectionally.
 * CONNECTED_CLIENT_READING:
 *     A giFT client has connected to us on port 1213, and is sending us
 *     its query.
 * CONNECTED_CLIENT_WRITING:
 *     We have received the query from the client, and are ready to, or are
 *     in the middle of, writing our response.
 * CONNECTED_CLIENT_FLUSHING:
 *     We have finished queueing our response to the client, and once it
 *     all actually gets written to the net, we will close this
 *     Connection.
 * CONNECTED_PEER_READING:
 *     A peer has connected to us on port 1214, and is sending us
 *     its HTTP request.
 * CONNECTED_PEER_WRITING:
 *     We have received the request from the peer, and are ready to, or are
 *     in the middle of, writing our response.
 * CONNECTED_PEER_FLUSHING:
 *     We have finished queueing our response to the peer, and once it
 *     all actually gets written to the net, we will close this
 *     Connection.
 * CONNECTED_PEER_STREAMING:
 *     We are in the middle of streaming a requested file to a peer.
 * CONNECTED_DOWNLOAD_CONNECTING:
 *     We are attempting to connect to a peer in order to download a
 *     file from it.
 * CONNECTED_DOWNLOAD_READING:
 *     We are reading the peer's response to our download request.
 * CONNECTED_DOWNLOAD_WAITING:
 *     We want to reconnect to this peer after some amount of time.
 *     This is the state we'll wait in until then.
 * CONNECTED_CLOSED:
 *     The connection to either the supernode or the client was closed,
 *     either due to the remote side shutting it down, or due to the
 *     communication being completed.  Once in this state, the
 *     Connection generally gets freed and becomes CONNECTED_NULL
 *     quickly.
 * CONNECTED_DEAD:
 *     An attempt to connect to a node or supernode failed.  We put the
 *     Connection in the CONNECTED_DEAD state, and have it stick around
 *     for a while, in order that we not try to connect to that same
 *     host again (at least not right away).
 */

typedef enum { CONNECTED_NULL, CONNECTED_NODE_CONNECTING,
    CONNECTED_NODE_CONNECTED, CONNECTED_SUPERNODE_CONNECTING,
    CONNECTED_SUPERNODE_WAITING, CONNECTED_SUPERNODE_SLURPING,
    CONNECTED_SUPERNODE_DECRYPTING, CONNECTED_SUPERNODE_CONNECTED,
    CONNECTED_CLIENT_READING, CONNECTED_CLIENT_WRITING,
    CONNECTED_CLIENT_FLUSHING, CONNECTED_PEER_READING,
    CONNECTED_PEER_WRITING, CONNECTED_PEER_FLUSHING, CONNECTED_PEER_STREAMING,
    CONNECTED_DOWNLOAD_CONNECTING, CONNECTED_DOWNLOAD_READING,
    CONNECTED_DOWNLOAD_WAITING, CONNECTED_CLOSED, CONNECTED_DEAD }
    ConnectionState;
#define NumConnectionState (CONNECTED_DEAD+1)

#include "giFTshare.h"

/**
 * An array of strings with human-readable versions of the
 * ConnectionStates.
 */
extern const char *connectionStateName[NumConnectionState];

/**
 * The timeouts, in seconds, for Connections in various states.  If a
 * Connection has no activity for longer than its timeout period, it
 * will be freed and set to CONNECTED_NULL.
 */
#define TIMEOUT_CONNECTING 10
#define TIMEOUT_NODE_CONNECTED 30
#define TIMEOUT_SUPERNODE_WAITING 60
#define TIMEOUT_SUPERNODE_SLURPING 4
#define TIMEOUT_SUPERNODE_DECRYPTING 30
#define TIMEOUT_SUPERNODE_CONNECTED 600
#define TIMEOUT_CLIENT_READING 10
#define TIMEOUT_CLIENT_WRITING 600
#define TIMEOUT_PEER_READING 30
#define TIMEOUT_PEER_WRITING 30
#define TIMEOUT_PEER_STREAMING 30
#define TIMEOUT_DOWNLOAD_CONNECTING 30
#define TIMEOUT_DOWNLOAD_READING 30
#define TIMEOUT_DEAD 3600

/**
 * The types of files we get statistics for.
 */
typedef enum { STATS_MISC=0x00, STATS_AUDIO=0x01, STATS_VIDEO=0x02,
    STATS_IMAGES=0x03, STATS_DOCUMENTS=0x04, STATS_SOFTWARE=0x05 }
    StatsFileType;
#define NumStatsFileType 6

/**
 * An array of strings with human-readable versions of the above types.
 */
extern const char *statsFileTypeName[NumStatsFileType];

/**
 * The statistics of the number of users, files, and bytes on the
 * network.
 */
typedef struct {
    time_t lastheard;
    size_t numUsers;
    size_t numTotFiles;
    size_t numTotGBytes;
    size_t numFiles[NumStatsFileType];
    size_t numGBytes[NumStatsFileType];
} SupernodeStats;

/* Callbacks of various types.  The "which" parameter to most of these
 * callbacks indicates the index into the supplied array of Connections
 * corresponding to the Connection associated with the data. */

/**
 * A callback of this type will be executed when the low level socket
 * reading routines have read and decrypted a complete packet.  The
 * type, size, and contents of the packet will be passed.  The contents
 * will have an extra NUL byte at the end (this NUL byte is not counted
 * as part of the length) in case you want to treat the contents as a
 * string.
 */
typedef int (*PacketHandler)(int which, int type, size_t size, byte *buf,
	void *context);

/**
 * A callback of this type will be executed just after a Connection
 * changes its state.  Note that if to == CONNECTION_NULL, then the
 * corresponding Connection pointer will have already been changed to
 * NULL, and you shouldn't try to dereference it.
 */
typedef int (*ConnectionChangedHandler)(int which, ConnectionState from,
	ConnectionState to, void *context);

/**
 * A callback of this type will be executed when some routine is
 * iterating over network addresses.  This is used, for example, in
 * parsing a list of peers, or when constructing a list of Connections
 * in the CONNECTED_SUPERNODE_CONNECTED state.
 */
typedef int (*AddressHandler)(int which, struct sockaddr *addr,
	socklen_t addrlen, void *context);

/**
 * A callback of this type will be executed when a client sends the giFT
 * daemon a query request.  The request (which starts with "<" and ends
 * with "/>") will be passed into this function.  It returns the
 * ConnectionState the client connection should be put in.  This will be
 * one of CONNECTED_CLIENT_WRITING (if we're planning to write more to
 * the client), CONNECTED_CLIENT_FLUSHING (if we've written everything
 * we need), or CONNECTED_CLOSED (if there's an error).
 */
typedef ConnectionState (*QueryHandler)(int which, char *buf, size_t size,
	void *context);

/**
 * A callback of this type is requested by the main program from the
 * packet layer so that it can use it to directly queue data for writing
 * to a client.
 */
typedef int (*ClientWriter)(const byte *buf, size_t size, void *context);

#include "giFTdownload.h"

/**
 * A callback of this type is used both to determine which chunk of
 * which file to download from any particular server, or to report that
 * a given chunk is avalable.  If buf == NULL, then *urip, *startp, and
 * *lenp will be filled in with information about the next chunk to
 * request.  In buf != NULL, then its contents (of length *lenp) will
 * be taken to be the received file contents, starting at file offset
 * *startp.  In the latter case, this handler should return < 0 if
 * this source should be disconnected (because, say, it has started
 * retrieving data we've already got).
 */
typedef int (*DownloadHandler)(int which, DownloadContext dlcontext,
	size_t dlid, char **urip, size_t *startp, size_t *lenp, byte *buf,
	void *context);

/**
 * The context which allows the Peer Server to make allocation
 * decisions.
 */
typedef struct {
    ShareHandle sh;
    char *username, *networkname;
    struct sockaddr_in server_addr;
    Connection *cs;
    size_t numcs;
    Connection *peers;
    size_t numpeers;
} PeerServerContext;

/* IO-level routines */

/**
 * Close a Connection and free all allocated state associated with it.
 */
void closeConnection(Connection *cp);

/**
 * Enqueue data for later writing to the socket.  Set needs_crypt to 1
 * if we need to encrypt it before writing it.  Set needs_crypt to 0
 * if this data isn't supposed to be encrypted at all.  You shouldn't be
 * passing already encrypted data to this function.
 */
void writeRawData(Connection c, const byte *buf, size_t len, int needs_crypt);

/**
 * Enqueue an unencrypted string for later writing to the socket.
 */
void writeRawString(Connection c, const char *s);

/**
 * Enqueue an entire packet (header and data) for later writing to a
 * socket.
 */
void writePacket(Connection c, int packet_type, byte *packet_data,
	size_t packet_len);

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
	struct timeval *tv);

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
	PeerServerContext *psc, DownloadHandler dh, void *dh_context);

/**
 * Add the number of each ConnectionState in the given range of
 * Connections to the running stats.
 */
void accumulateStats(Connection *cs, size_t nconn, int stats[]);

/**
 * Compare the address for the given Connection to the given one.
 */
int cmpConnectionAddr(Connection c, struct sockaddr *addr,
	socklen_t addrlen);

/**
 * Add the given address to the connection list.  Return 0 if we don't
 * want to add it, 1 if we added it, and -1 if we'd like to add it, but
 * we're full.
 */
int addToConnectionList(Connection *cs, size_t nconn, size_t maxconn,
	struct sockaddr *addr, socklen_t addrlen, char *username,
	char *networkname, struct sockaddr_in *server_addr);

/**
 * Get the list of fully-connected nodes.  Each one will be passed in
 * turn to the AddressHandler.
 */
void getConnecteds(Connection *cs, size_t nconn, AddressHandler ah,
	void *ah_context);

/**
 * Report the statistics from an active Connection.
 */
int reportStatsFromConnection(Connection c, SupernodeStats *s);

/**
 * Add a new client to a Connection list.
 */
int addToClientsList(Connection *cs, size_t nconn, struct sockaddr *addr,
	socklen_t addrlen, int fd);

/**
 * Set the query id for a client Connection.  Also increment a reference
 * count to keep track of the number of times this has been called for
 * this Connection.
 */
void setClientID(Connection c, unsigned short qid);

/**
 * Indicate that we're done queueing data for the client with the given
 * id.  Decrement the reference count, and when it reaches 0, really
 * flush the connection.
 */
void flushClientConnection(Connection *clients, size_t nconn,
	unsigned short qid, ConnectionChangedHandler cch, void *cch_context);

/**
 * Get a ClientWriter associated with the given query id.
 */
int getClientWriterByQid(Connection *clients, size_t nconn,
	unsigned short qid, ClientWriter *cwp, void **cw_contextp);

/**
 * Get a ClientWriter for a given client Connection.
 */
int getClientWriterByConnection(Connection client, ClientWriter *cwp,
	void **cw_contextp);

/**
 * Free a ClientWriter context.
 */
void freeCWContext(void **cw_contextp);

/**
 * Add a new peer to a Connection list.
 */
int addToPeersList(Connection *cs, size_t nconn, struct sockaddr *addr,
	socklen_t addrlen, int fd);

/**
 * Get a FILE* ready for streaming to a given Connection.  We start
 * streaming from its current file position, for up to length bytes.
 */
int startStreamFile(Connection c, FILE *fp, size_t length);

/**
 * Initiate a connection with a peer in order to download data from it.
 */
Connection newDownloadConnection(DownloadContext dc, size_t dlid,
	struct sockaddr *nodeaddr, socklen_t addrlen);

#endif
