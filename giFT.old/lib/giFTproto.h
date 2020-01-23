#ifndef __giFTPROTO_H__
#define __giFTPROTO_H__

#include <netinet/in.h>
#include "giFTpacket.h"

/**
 * The known tag types.
 */
enum { FILE_TAG_ANY = 0x00, FILE_TAG_YEAR = 0x01, FILE_TAG_HREF = 0x02,
    FILE_TAG_HASH = 0x03, FILE_TAG_TITLE = 0x04, FILE_TAG_TIME = 0x05,
    FILE_TAG_ARTIST = 0x06, FILE_TAG_ALBUM = 0x08, FILE_TAG_LANGUAGE = 0x0a,
    FILE_TAG_KEYWORDS = 0x0c, FILE_TAG_RESOLUTION = 0x0d,
    FILE_TAG_GENRE = 0x0e, FILE_TAG_BITDEPTH = 0x11, FILE_TAG_QUALITY = 0x15,
    FILE_TAG_VERSION = 0x18, FILE_TAG_COMMENT = 0x1a, FILE_TAG_RATING = 0x1d,
    FILE_TAG_SIZE = 0x21 };

/**
 * One "tag" for one file in a file description.  Each file usually has
 * a number of these.  The tag specifies what the data represents
 * (filename, title, artist, resolution, etc.).  The data pointer is
 * NULL if the length is 0, or if for some reason memory could not be
 * allocated, so check it before using it.
 */
typedef struct {
    unsigned int tag;
    size_t len;
    byte *data;
} FileTag;

/**
 * A description of a file.  Used in a number of different parts of the
 * protocol.  One of these contains a number of the above FileTags.
 */
typedef struct {
    byte hash[20];
    size_t checksum;
    size_t filesize;
    size_t ntags;
    FileTag *tags;
} FileDescription;

/**
 * One file in the search result.  A record of this type should be
 * ignored unless "valid" is 1.  This record contains a FileDescription,
 * as outlined above.
 */
typedef struct {
    int valid;
    struct sockaddr_in host;
    byte bandwidthtag;
    char *username;
    char *networkname;
    FileDescription d;
} FileEntry;

/**
 * A response to a search query, which contains 0 or more FileEntrys.
 * The id field indicates which query this is a response to.
 */
typedef struct s_QueryResponse {
    struct sockaddr_in addr;
    unsigned short id;
    size_t numresp;
    FileEntry *files;
} *QueryResponse;

/**
 * The valid realms for a complex query.
 */
typedef enum { QUERY_REALM_EVERYTHING=0x3f, QUERY_REALM_AUDIO=0x21,
    QUERY_REALM_VIDEO=0x22, QUERY_REALM_IMAGES=0x23,
    QUERY_REALM_DOCUMENTS=0x24, QUERY_REALM_SOFTWARE=0x25 } QueryRealm;

/**
 * The valid types of searches in a complex query.
 */
typedef enum { QUERY_EQUALS=0x00, QUERY_ATMOST=0x02, QUERY_APPROX=0x03,
    QUERY_ATLEAST=0x04, QUERY_SUBSTRING=0x05 } QueryTermType;

/**
 * A term in a complex query.
 */
typedef struct {
    QueryTermType type;
    byte field;
    size_t len;
    byte *data;
} QueryTerm;

/**
 * The data structure representing a complex query.
 */
typedef struct s_ComplexQuery {
    unsigned short maxresults;
    unsigned short qid;
    QueryRealm realm;
    size_t numterms;
    QueryTerm *terms;
} *ComplexQuery;

/**
 * Initialize the giFT library.  Call this before calling any other giFT
 * functions.
 */
void giFTInit(void);

/* Routines to send various kinds of packets */

/**
 * Send a query packet with the given query id for the given string.
 * The string must be less than 16K in length.  The query id should
 * always be non-zero.
 */
int sendQuery(Connection c, unsigned short id, char *query);

/**
 * Send a compiled query packet to the given Connection.
 */
int sendComplexQuery(Connection c, byte *compiled_query, size_t len);

/* Routines to parse various kinds of packets */

/**
 * Parse a packet containing a list of peers (types 0x00 and 0x02).
 * The supplied AddressHandler callback will be called once for each
 * peer found in the list.
 */
void readNodeList(int type, size_t size, byte *buf, AddressHandler ah,
	void *ah_context);

/**
 * Parse a packet containing a response to a query (type 0x07).  This
 * routine will pass back a pointer to a newly allocated QueryResponse
 * object.  Be sure to call freeQueryResponse when you're done with it.
 */
QueryResponse readQueryResponse(int type, size_t size, byte *buf);

/**
 * Parse a "no more search results" packet (type 0x08).  This routine
 * returns the relevant query id.
 */
unsigned short readQueryTerminated(int type, size_t size, byte *buf);

/* Miscellaneous routines */

/**
 * Free the memory associated with a QueryResponse object.  Pass the
 * address of the variable storing the result returned from
 * readQueryResponse.  The memory will be freed, and the variable will
 * be set to NULL.
 */
void freeQueryResponse(QueryResponse *qrp);

/**
 * Create a Complex Query, initially with no query terms.
 */
ComplexQuery createComplexQuery(unsigned short maxresults, unsigned short qid);

/**
 * Free the memory allocated to a complex query.
 */
void freeComplexQuery(ComplexQuery *cqp);

/**
 * Add a QueryTerm to a complex query.  Returns the new number of terms
 * in the query, or -1 on error.
 */
int buildComplexQuery(ComplexQuery cq, QueryTermType type, byte field,
	size_t len, byte *data);

/**
 * Compile a given ComplexQuery for later sending.  Be sure to free()
 * the result when you're done.
 */
byte* compileComplexQuery(ComplexQuery cq, size_t *outmsglenp);

#endif
