#ifndef __giFTCRYPT_H__
#define __giFTCRYPT_H__

#define PADSIZE 63

#include "giFT.h"

/**
 * The internal state of the stream cipher.  This state gets updated for
 * every byte encrypted or decrypted.
 */
typedef struct {
    int pos;
    byte pad[PADSIZE];
} CipherState;

/**
 * Encrypt or decrypt a region of data.  Since this is a stream cipher,
 * the operations of encryption and decryption are in fact the same.
 */
void do_crypt(CipherState *cs, byte *data, size_t len);

/**
 * Given an encrypted list of peers, break the encryption and determine
 * the CipherState.
 */
int decrypt_node_list(byte *msg, size_t msglen, CipherState *csp);

/**
 * Given an encrypted list of PONGS, break the encryption and determine
 * the CipherState.
 */
int decrypt_pongs(byte *msg, size_t msglen, CipherState *csp);

/**
 * Rewind the given CipherState to try to decrypt old data.
 */
int decrypt_old_data(byte *oldbuf, size_t len, byte *newbuf, size_t newlen,
	CipherState *csp);

#endif
