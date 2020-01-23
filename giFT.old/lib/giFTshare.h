#ifndef __giFTSHARE_H__
#define __giFTSHARE_H__

/**
 * A handle representing a list of files being shared.
 */
typedef struct s_ShareHandle *ShareHandle;

/**
 * Create a new ShareHandle, which references no files.
 */
ShareHandle shareNewHandle(const char *sharedir);

/**
 * Free the memory allocated to a ShareHandle.
 */
void shareFreeHandle(ShareHandle *shp);

/**
 * Send the current list of shared files to the given Connection, which
 * is presumably a new one.
 */
int shareSendAll(ShareHandle h, Connection c);

/**
 * Update the current list of shared files and send the changes to all
 * of the established Connections in the list.
 */
int shareUpdate(ShareHandle h, Connection *cs, size_t nconn);

/**
 * Return the eventual output length of this file list.  mode == 0 for a
 * textual output, 1 for a binary output.
 */
int shareGetTotalLength(ShareHandle sh, int mode);

/**
 * Get the length and type of the speficied file, and return a handle to
 * it, which will be passed back to shareWriteFile later (but before any
 * modifications to the ShareHandle can be done).
 */
void *shareGetFileLengthAndType(ShareHandle sh, const char *uri,
	int *clenp, const char **ctypep);

/**
 * Write a file list to the given Connection.  mode is as in
 * shareGetTotalLength, above.
 */
ConnectionState shareWriteFileList(ShareHandle sh, Connection c, int mode);

/**
 * Arrange that the given portion of the given file is written to the
 * given Connection.
 */
ConnectionState shareWriteFile(ShareHandle sh, Connection c, void *handle,
	int rangestart, int length);

#endif
