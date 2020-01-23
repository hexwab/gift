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
#include "giFTcrypt.h"

/* A matrix with entries mod 256 */
typedef struct {
    int ncols, nrows;
    byte *data;
} Matrix;

static byte evenness[256];

/**
 * Make the 'evenness' table; evenness[x] is the exponent on the highest
 * power of 2 dividing x
 */
static void init_evenness(void)
{
    int i,j;
    for(j=0;j<256;++j) evenness[j] = 0;
    for(i=1;i<=8;++i) {
	for(j=0;j<256;j+=(1<<i)) {
	    ++evenness[j];
	}
    }
}

static byte inverse[256];

/**
 * Make the 'inverse' table, where inverse[x] is the inverse (mod 256) of the
 * largest odd factor of x.
 */
static void init_inverse(void)
{
    int i,j;

    inverse[0] = 0;
    for(i=1;i<256;++i) {
	if (evenness[i] > 0) {
	    /* This is an even number, so just look up the inverse of
	     * its largest odd factor (== (i >> evenness[i])), which
	     * we've already calculated */
	    inverse[i] = inverse[i>>evenness[i]];
	} else {
	    /* This is an odd number; just brute-force the answer */
	    for(j=1;j<256;j+=2) {
		byte b = i*j;
		if (b==1) {
		    inverse[i] = j;
		    break;
		}
	    }
	}
    }
}

/**
 * Make the solver matrix for a given (sorted) list of known plaintext
 * positions.
 */
static int makeMatrix(Matrix *mp, int *known_positions,
	size_t num_known_positions)
{
    static int is_setup = 0;
    int r,c;
    int cur, used, bnum;
    int retval = -1;
    byte *pad = malloc(PADSIZE*PADSIZE);
    byte *table = malloc((PADSIZE+num_known_positions)*num_known_positions);
    mp->nrows = 0;
    mp->ncols = 0;
    mp->data = NULL;

    if (!is_setup) {
	is_setup = 1;
	init_evenness();
	init_inverse();
    }

    if (pad == NULL || table == NULL) {
	fprintf(stderr, "Out of memory at %s:%d\n", __FILE__, __LINE__);
	goto clean;
    }
    if (num_known_positions < PADSIZE) {
	fprintf(stderr, "Too few known positions\n");
	goto clean;
    }

    /* Make the identity matrix */
    for(r=0;r<PADSIZE;++r) {
	for(c=0;c<PADSIZE;++c) {
	    pad[r*PADSIZE+c] = (r == c ? 1 : 0);
	}
    }

    cur = 0;
    used = 0;
    bnum = 0;

    while(used < num_known_positions) {
	int lastcur = (cur == 0 ? PADSIZE-1 : cur-1);
	int nextcur = (cur+1)%PADSIZE;
	int i;

	/* Update the pad */
	for(i=0;i<PADSIZE;++i) {
	    pad[cur*PADSIZE+i] += pad[lastcur*PADSIZE+i];
	}

	/* Do we use this one? */
	if (bnum == known_positions[used]) {
	    /* Make row 'used' of the table */
	    for(i=0;i<PADSIZE;++i) {
		table[used*(PADSIZE+num_known_positions)+i] =
		    pad[cur*PADSIZE+i];
	    }
	    for(i=0;i<num_known_positions;++i) {
		table[used*(PADSIZE+num_known_positions)+PADSIZE+i] =
		    (used == i ? 1 : 0);
	    }
	    ++used;
	}
	cur = nextcur;
	++bnum;
    }

    /* OK, we've generated the matrix.  Now invert it. */
    for(r=0;r<PADSIZE;++r) {
	int i;

	/* First pivot, if necessary */
	int mineven = evenness[table[r*(PADSIZE+num_known_positions)+r]];
	int minwhere = r;
	byte multiplier;

	/* fprintf(stderr,"Doing row %d... ", r); */

	for(i=r+1;i<num_known_positions;++i) {
	    if (evenness[table[i*(PADSIZE+num_known_positions)+r]] < mineven) {
		mineven = evenness[table[i*(PADSIZE+num_known_positions)+r]];
		minwhere = i;
	    }
	}
	if (mineven != 0) {
	    fprintf(stderr,"*** Column of noninvertible elements at %d!\n\n",
		    r);
	    goto clean;
	}
	if (minwhere != r) {
	    for(i=0;i<PADSIZE+num_known_positions;++i) {
		byte temp = table[r*(PADSIZE+num_known_positions)+i];
		table[r*(PADSIZE+num_known_positions)+i] =
		    table[minwhere*(PADSIZE+num_known_positions)+i];
		table[minwhere*(PADSIZE+num_known_positions)+i] = temp;
	    }
	    /* fprintf(stderr,"pivot with row %d\n", minwhere); */
	} else {
	    /* fprintf(stderr,"pivot ok\n"); */
	}

	/* Now make this row start with a power of 2 (which will be the
	 * lowest power from among all available rows */
	multiplier = inverse[table[r*(PADSIZE+num_known_positions)+r]];
	for(i=r;i<PADSIZE+num_known_positions;++i) {
	    table[r*(PADSIZE+num_known_positions)+i] *= multiplier;
	}

	/* Now for each other row in the table (above and below),
	 * subtract the appropriate multiple of this row */
	for(i=0;i<num_known_positions;++i) {
	    int j;

	    if (i==r) continue;
	    multiplier = -(table[i*(PADSIZE+num_known_positions)+r] >>mineven);
	    for(j=r;j<PADSIZE+num_known_positions;++j) {
		table[i*(PADSIZE+num_known_positions)+j] +=
		    multiplier * table[r*(PADSIZE+num_known_positions)+j];
	    }
	}
    }

    /* OK, we've inverted it.  Save the matrix. */
    mp->nrows = num_known_positions;
    mp->ncols = num_known_positions;
    mp->data = malloc((mp->nrows)*(mp->ncols));
    for(r=0;r<mp->nrows;++r) for(c=0;c<mp->ncols;++c) {
	mp->data[r*(mp->ncols)+c] =
	    table[r*(PADSIZE+num_known_positions)+PADSIZE+c];
    }
    retval = 0;

clean:
    if (pad) free(pad);
    if (table) free(table);
    return retval;
}

/**
 * Free the storage allocated to the given Matrix.  Note that this
 * routine is never actually called at the moment, since we just use
 * one statically generated Matrix, and keep it around forever.  If in
 * the future we find we need multiple matrices, we'll need to free up
 * old ones with this.
 */
static void freeMatrix(Matrix *mp)
{
    if (mp->data != NULL) free(mp->data);
    mp->data = NULL;
}

/**
 * The comparison routine used in the "sort" portion of the state update
 * function (see below).  It's pretty much just an unsigned comparison
 * of bytes, except that bit 5 of each byte is inverted before
 * comparison for some reason.
 */
static int rekey_cmp(const void *ap, const void *bp)
{
    int a = (int)*(byte *)ap;
    int b = (int)*(byte *)bp;

    return ((a^0x20)-(b^0x20));
}

/**
 * Update the state of the cipher, and output the keystream byte which
 * will be XOR'ed with the plaintext to produce the ciphertext, or with
 * the ciphertext to produce the plaintext.  (The same byte is of course
 * produced in either case.)  If for some reason you want the
 * not quite right, but completely linear, version of the cipher, set
 * do_rekey to 0.
 */
static byte clock_cipher(CipherState *sp, int do_rekey, int *did_rekey)
{
    byte xor;

    /* Get the previous position of the state, wrapping around if
     * necessary. */
    unsigned char lastpos = (sp->pos > 0 ? sp->pos-1 : PADSIZE-1);

    /* Add the value at the previous position to the one at the current
     * position.  This creates a "running total". */
    sp->pad[sp->pos] += sp->pad[lastpos];

    /* We're going to output the current value of the "running total". */
    xor = sp->pad[sp->pos];

    /* But before we output it, see if we should mangle the internal
     * state a bit.  We do this with probability 1/8 (the high three
     * bits of the xor byte are all 0), every PADSIZE output bytes.
     * The mangling involves two steps:
     * - Sort a particular 5 bytes of the state according to the above
     *   rekey_cmp comparison function.
     * - Modify the value of every third entry (starting with entry 5)
     */
    if (do_rekey && sp->pos == 7 && ((xor & 0x70) == 0)) {
	int i;
	/* Which 5 elements should we sort?  We calculate this in a
	 * pretty odd manner. */
	int sortpos = xor + sp->pad[2];
	sortpos = ((sortpos * sortpos) + 2) % (PADSIZE-4);

	/* Sort those 5 elements according to the rekey_cmp comparison
	 * function. */
	qsort(sp->pad + sortpos, 5, 1, rekey_cmp);

	/* Now modify every third byte of the state in a simple way. */
	for(i=5;i<PADSIZE;i+=3) {
	    byte val = sp->pad[i];
	    val = ~val + i;
	    sp->pad[i] = val|1;
	}
	if (did_rekey) *did_rekey = 1;
    }

    /* Increment the current position of the state, wrapping around if
     * necessary. */
    if (++(sp->pos) == PADSIZE) {
	sp->pos = 0;
    }
    return xor;
}

/**
 * Test a guess at the initial cipher state.  Returns 0 on failure,
 * 1 on success.  We test it by trying to decrypt, and if we don't get 8
 * unexpected errors in a row, we guess that we're doing fine.  The 8
 * errors allows us to be slightly wrong in our guess at known
 * plaintext.
 */
static int testState(CipherState *sp, byte *ciphertext,
	size_t cipherlen, int *known_positions, byte *known_bytes,
	size_t num_known, int fudge)
{
    int bpos = 0;
    int known_seen = 0;
    int missed = 0;
    while(bpos < cipherlen && known_seen < num_known) {
	int rekey = 0;
	byte xor = clock_cipher(sp, 1, &rekey);
	if (bpos == known_positions[known_seen]) {
	    if ((xor ^ known_bytes[known_seen]) != ciphertext[bpos]) {
		/* Bad encryption */
		++missed;
		if (missed >= fudge) return 0;
	    } else {
		missed = 0;
	    }
	    ++known_seen;
	}
	++bpos;
    }
    return 1;
}

/**
 * Determine the CipherState given the encrypted message and a bit of
 * (mostly) known plaintext.  Return 0 on success, -1 on failure.
 */
static int findState(Matrix *mp, CipherState *sp, byte *ciphertext,
	size_t cipherlen, int *known_positions, byte *known_bytes,
	size_t num_known, int fudge)
{
    int i,j;
    int retval = -1;
    CipherState found;

    if (num_known < mp->ncols) {
	fprintf(stderr, "Not enough known text\n");
	goto clean;
    }

    /* See if there was a rekey in the span we care about */
    for(i=PADSIZE;i<mp->nrows;++i) {
	byte check = 0;
	for(j=0;j<mp->ncols;++j) {
	    check += mp->data[i*(mp->ncols)+j] *
		(ciphertext[known_positions[j]] ^ known_bytes[j]);
	}
	if (check != 0) {
	    /* fprintf(stderr, "Failed to solve for state\n"); */
	    goto clean;
	}
    }

    /* OK, no rekey detected.  Solve for the state */
    for(i=0;i<PADSIZE;++i) {
	byte pval = 0;
	for(j=0;j<mp->ncols;++j) {
	    pval += mp->data[i*(mp->ncols)+j] *
		(ciphertext[known_positions[j]] ^ known_bytes[j]);
	}
	sp->pad[i] = pval;
    }

    /* Now we have to sync up the absolute position of the circular pad.
     * We could be clever, and try to spot potential rekey positions,
     * but we won't bother, and we just brute-force it. */
    found.pos = -1;
    for(i=0;i<PADSIZE;++i) {
	CipherState trial;
	trial.pos = i;
	memmove(trial.pad+i, sp->pad, PADSIZE-i);
	memmove(trial.pad, sp->pad+PADSIZE-i, i);
	if (testState(&trial, ciphertext, cipherlen, known_positions,
		    known_bytes, num_known, fudge)) {
	    if (found.pos == -1) {
		found.pos = i;
		memmove(found.pad+i, sp->pad, PADSIZE-i);
		memmove(found.pad, sp->pad+PADSIZE-i, i);
	    } else {
		/* fprintf(stderr, "Ambiguous pad position\n"); */
		goto clean;
	    }
	}
    }
    if (found.pos == -1) {
	/* fprintf(stderr, "Could not find pad position\n"); */
	goto clean;
    }

    /* That should do it. */
    *sp = found;

    retval = 0;

clean:
    return retval;
}

/**
 * Encrypt or decrypt a region of data.  Since this is a stream cipher,
 * the operations of encryption and decryption are in fact the same.
 */
void do_crypt(CipherState *cs, byte *data, size_t len)
{
    if (cs == NULL) return;

    while(len) {
	byte xor = clock_cipher(cs, 1, NULL);
	*data ^= xor;
	++data;
	--len;
    }
}

static int node_matrix_init = 0;
static Matrix node_matrix;
static int node_known_pos[400];
static byte node_known_bytes[400];

/**
 * Given an encrypted list of peers, break the encryption and determine
 * the CipherState.
 */
int decrypt_node_list(byte *msg, size_t msglen, CipherState *csp)
{
    int res;
    int found = 0;
    size_t offset = 0;
    size_t i;

    if (node_matrix_init == 0) {
	/* Create the node matrix */
	for(i=0;i<200;++i) {
	    node_known_pos[2*i]   = 8*i+4; node_known_bytes[2*i]   = 0x04;
	    node_known_pos[2*i+1] = 8*i+5; node_known_bytes[2*i+1] = 0xBE;
	}
	res = makeMatrix(&node_matrix, node_known_pos, 66);
	if (res) {
	    return -1;
	}
	node_matrix_init = 1;
    }

    while(!found && msglen-offset > 300) {
	res = findState(&node_matrix, csp, msg+offset, msglen-offset,
		node_known_pos, node_known_bytes, 400, 8);
	if (res == 0) {
	    /* Found it! */
	    found = 1;
	} else {
	    offset += 1;
	}
    }
    if (!found) return -1;

    /* Decrypt the message, now that we have recovered the CipherState.
     * This syncs up the CipherState to its correct position. */
    do_crypt(csp, msg+offset, msglen-offset);

    return (msglen-offset);
}

static int pong_matrix_init = 0;
static Matrix pong_matrix;
static int pong_known_pos[4000];
static byte pong_known_bytes[4000];

/**
 * Given an encrypted list of PONGS, break the encryption and determine
 * the CipherState.
 */
int decrypt_pongs(byte *msg, size_t msglen, CipherState *csp)
{
    int res;
    int found = 0;
    size_t offset = 0;
    size_t i;

    if (pong_matrix_init == 0) {
	/* Create the pong matrix */
	for(i=0;i<4000;++i) {
	    pong_known_pos[i] = i; pong_known_bytes[i] = 0x52;
	}
	res = makeMatrix(&pong_matrix, pong_known_pos, 66);
	if (res) {
	    return -1;
	}
	pong_matrix_init = 1;
    }

    while(!found && msglen-offset > 300) {
	size_t sz = msglen - offset;
	if (sz > 4000) sz = 4000;
	res = findState(&pong_matrix, csp, msg+offset, msglen-offset,
		pong_known_pos, pong_known_bytes, sz, 0);
	if (res == 0) {
	    /* Found it! */
	    found = 1;
	} else {
	    offset += 1;
	}
    }
    if (!found) return -1;

    return (msglen-offset);
}

/**
 * Rewind the given CipherState to try to decrypt old data.
 */
int decrypt_old_data(byte *oldbuf, size_t len, byte *newbuf, size_t newlen,
	CipherState *csp)
{
    CipherState cs;
    int i;

    if (len == 0) return 1;
    if (oldbuf == NULL || csp == NULL) return -1;
    cs = *csp;

    /* Try to rewind it */
    for(i=0;i<len;++i) {
	int curpos = cs.pos;
	int oneago = (PADSIZE-1+curpos)%PADSIZE;
	int twoago = (PADSIZE-2+curpos)%PADSIZE;
	cs.pad[oneago] -= cs.pad[twoago];
	cs.pos = oneago;
    }

    /* Encrypt the result */
    for(i=0;i<len;++i) {
	oldbuf[i] ^= clock_cipher(&cs, 0, NULL);
    }

    /* Decrypt the newbuf, now that we have recovered the CipherState.
     * This syncs up the CipherState to its correct position. */
    do_crypt(&cs, newbuf, newlen);

    *csp = cs;

    return 1;
}
