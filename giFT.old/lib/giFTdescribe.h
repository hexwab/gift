#ifndef __giFTDESCRIBE_H__
#define __giFTDESCRIBE_H__

#include <sys/stat.h>
#include "giFTproto.h"

/**
 * A table entry used for determining type information from suffixes.
 * The handler should be used to add type-specific tags to the
 * FileDescription.  It should return 0 on success or -1 on failure.
 */
typedef struct {
    char *suffix;
    char *mimetype;
    StatsFileType ftype;
    int (*handler)(FileDescription *fdp, const char *filename, FILE *fp,
	    struct stat *st, void *context);
    void *context;
} SuffixTableEntry;
extern const SuffixTableEntry describeSuffixTable[];

/**
 * Describe a given file into a FileDescription structure.  Its type for
 * statistics purposes should be returned in *ftype.  Return 0 if
 * everything went well, -1 on error.
 */
int describeFile(FileDescription *fdp, StatsFileType *ftype,
	const char **contenttypep, const char *filename, FILE *fp,
	struct stat *st);

/**
 * Add a tag to a given FileDescription.
 */
int describeAddDataTag(FileDescription *fdp, int tag, const byte *data,
	size_t len);

/**
 * Add an encoded integer as a tag to a given FileDescription.
 */
int describeAddIntTag(FileDescription *fdp, int tag, size_t v);

/**
 * Add a resolution tag to a given FileDescription.
 */
int describeAddResolutionTag(FileDescription *fdp, size_t width,
	size_t height);

#endif
