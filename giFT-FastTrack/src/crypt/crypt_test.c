
#include <stdio.h>
#include <math.h>
#include <limits.h>
#include <string.h>
#include <stdlib.h>
#include "md5.h"

extern void enc_type_1 (unsigned char *out_key, unsigned char *in_key);
extern void enc_type_2 (unsigned int *key, unsigned int seed);
extern void enc_type_20 (unsigned int *key, unsigned int seed);
extern void enc_type_80 (unsigned int *key, unsigned int seed);

unsigned char enc_1_md5_in[]   = "\x28\x5f\x6e\x27\xf0\x42\xca\x78\x5a\x0b\x21\x4b\x21\x78\x3a\x78";
unsigned char enc_1_md5_out[]  = "\xfc\x2f\x97\xe5\x04\xf5\x43\x0b\xd6\x10\x45\x00\x82\x71\x82\x12";
unsigned char enc_2_md5_in[]   = "\x9d\x2a\x30\xae\x28\xc1\x3e\x3b\x91\x88\x4d\xfb\xf3\x98\x65\x61";
unsigned char enc_2_md5_out[]  = "\xd3\x10\xb6\x45\x9a\x86\x88\x4a\xac\x6c\x43\x66\xc8\xf0\xa6\x19";
unsigned char enc_20_md5_in[]  = "\x9d\x2a\x30\xae\x28\xc1\x3e\x3b\x91\x88\x4d\xfb\xf3\x98\x65\x61";
unsigned char enc_20_md5_out[] = "\x44\x29\x82\x10\xa3\x6e\x14\x3a\xd2\x8a\xa2\x82\x6d\xe7\x11\xe6";
unsigned char enc_80_md5_in[]  = "\x9d\x2a\x30\xae\x28\xc1\x3e\x3b\x91\x88\x4d\xfb\xf3\x98\x65\x61";
unsigned char enc_80_md5_out[] = "\x46\x47\xd6\xf8\x65\x72\xfe\x79\xaf\x48\xc9\x8c\x17\xf7\xd6\x5f";

/* rndlcg            Linear Congruential Method, the "minimal standard generator"
                     Park & Miller, 1988, Comm of the ACM, 31(10), pp. 1192-1201

*/

static int quotient  = INT_MAX / 16807L;
static int remain = INT_MAX % 16807L;
static int seed_val = 1L;

static unsigned int randlcg()       /* returns a random unsigned integer */
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

/* r250.c	the r250 uniform random number algorithm

		Kirkpatrick, S., and E. Stoll, 1981; "A Very Fast
		Shift-Register Sequence Random Number Generator",
		Journal of Computational Physics, V.40

		also:

		see W.L. Maier, DDJ May 1991
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

static unsigned int r250()		/* returns a random unsigned integer */
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

static void reverse_bytes (unsigned int *buf, unsigned int longs)
{
	unsigned char *cbuf = (unsigned char*)buf;

	for ( ; longs; longs--, buf++, cbuf += 4)
	{
		*buf = ( (unsigned int) cbuf[3] << 8 | cbuf[2]) << 16 |
			   ( (unsigned int) cbuf[1] << 8 | cbuf[0]);
	}
}

static char *md5_get_str (unsigned char *hash)
{
	static const char hex_string[] = "0123456789abcdef";
	static char string[MD5_HASH_LEN*2+1];
	char *p = string;
	int i;

	if(!hash)
		return NULL;

	for(i=0; i<MD5_HASH_LEN; i++, p+=2)
	{
		p[0] = hex_string[hash[i] >> 4];
		p[1] = hex_string[hash[i] & 0x0F];
	}

	string[MD5_HASH_LEN*2] = 0;

	return string;
}

int main (int argc, char* argv[])
{
	MD5Context in_md5_ctx, out_md5_ctx;
	unsigned char in_hash[MD5_HASH_LEN], out_hash[MD5_HASH_LEN];
	int runs, i, exit_code = 0, test_all = argc == 1;
	unsigned int key_80[20];
	unsigned int seed;
	unsigned int key_256_in[64], key_256_out[64];

	if (argc == 2 && !atoi (argv[1]))
	{
		printf ("usage: %s [enc_type]\n", argv[0]);
		printf ("where enc_type is one if the following:\n");
		printf ("- \"1\" for enc_type_1\n- \"2\" for enc_type_2\n- \"20\" for enc_type_20\n- \"80\" for enc_type_80\n");
		printf ("no argument means all enc_types are tested.\n");
		return 1;
	}

	if (test_all || (argc == 2 && !strcmp (argv[1], "1")))
	{
		printf ("\ntesting enc_type_1 with 1.000 iterations...\n");
		printf ("-------------------------------------------------------------\n");

		r250_init(1);
		MD5Init (&in_md5_ctx);
		MD5Init (&out_md5_ctx);

		for (runs=0; runs < 1000; runs++)
		{
			/* fill input with randomness */
			for (i=0; i<64; i++)
				key_256_in[i] = r250();
			reverse_bytes (key_256_in, 64);
			MD5Update (&in_md5_ctx, (unsigned char*)key_256_in, 255);
			// only 255 bytes are used by enc_type_1
			enc_type_1 ((unsigned char*)key_256_out, (unsigned char*)key_256_in);
			MD5Update (&out_md5_ctx, (unsigned char*)key_256_out, 255);		
		}

		MD5Final (in_hash, &in_md5_ctx);
		MD5Final (out_hash, &out_md5_ctx);

		printf ("input hash is:  %s\n", md5_get_str (in_hash));
		printf ("should be:      %s", md5_get_str (enc_1_md5_in));
		if (memcmp (in_hash, enc_1_md5_in, MD5_HASH_LEN) == 0)
			printf (" => OK\n\n");
		else
			printf (" => FAILURE, PRNG broken?\n\n"), exit_code++;

		printf ("output hash is: %s\n", md5_get_str (out_hash));
		printf ("should be:      %s", md5_get_str (enc_1_md5_out));
		if (memcmp (out_hash, enc_1_md5_out, MD5_HASH_LEN) == 0)
			printf (" => OK\n");
		else
			printf (" => FAILURE\n"), exit_code++;
	}

	if (test_all || (argc == 2 && !strcmp (argv[1], "2")))
	{
		printf ("\ntesting enc_type_2 with 500.000 iterations...\n");
		printf ("-------------------------------------------------------------\n");

		r250_init(1);
		MD5Init (&in_md5_ctx);
		MD5Init (&out_md5_ctx);

		for (runs=0; runs < 500000; runs++)
		{
			/* fill input with randomness */
			for (i=0; i<20; i++)
				key_80[i] = r250();	
			seed = r250();

			reverse_bytes (key_80, 20);
			MD5Update (&in_md5_ctx, (unsigned char*)key_80, 80);
			reverse_bytes (key_80, 20);

			reverse_bytes (&seed, 1);
			MD5Update (&in_md5_ctx, (unsigned char*)&seed, 4);
			reverse_bytes (&seed, 1);

			enc_type_2 (key_80, seed);

			reverse_bytes (key_80, 20);
			MD5Update (&out_md5_ctx, (unsigned char*)key_80, 80);		
			reverse_bytes (key_80, 20);
		}

		MD5Final (in_hash, &in_md5_ctx);
		MD5Final (out_hash, &out_md5_ctx);

		printf ("input hash is:  %s\n", md5_get_str (in_hash));
		printf ("should be:      %s", md5_get_str (enc_2_md5_in));
		if (memcmp (in_hash, enc_2_md5_in, MD5_HASH_LEN) == 0)
			printf (" => OK\n\n");
		else
			printf (" => FAILURE, PRNG broken?\n\n"), exit_code++;

		printf ("output hash is: %s\n", md5_get_str (out_hash));
		printf ("should be:      %s", md5_get_str (enc_2_md5_out));
		if (memcmp (out_hash, enc_2_md5_out, MD5_HASH_LEN) == 0)
			printf (" => OK\n");
		else
			printf (" => FAILURE\n"), exit_code++;
	}

	if (test_all || (argc == 2 && !strcmp (argv[1], "20")))
	{
		printf ("\ntesting enc_type_20 with 500.000 iterations...\n");
		printf ("-------------------------------------------------------------\n");

		r250_init(1);
		MD5Init (&in_md5_ctx);
		MD5Init (&out_md5_ctx);

		for (runs=0; runs < 500000; runs++)
		{
			/* fill input with randomness */
			for (i=0; i<20; i++)
				key_80[i] = r250();	
			seed = r250();

			reverse_bytes (key_80, 20);
			MD5Update (&in_md5_ctx, (unsigned char*)key_80, 80);
			reverse_bytes (key_80, 20);

			reverse_bytes (&seed, 1);
			MD5Update (&in_md5_ctx, (unsigned char*)&seed, 4);
			reverse_bytes (&seed, 1);

			enc_type_20 (key_80, seed);

			reverse_bytes (key_80, 20);
			MD5Update (&out_md5_ctx, (unsigned char*)key_80, 80);		
			reverse_bytes (key_80, 20);
		}

		MD5Final (in_hash, &in_md5_ctx);
		MD5Final (out_hash, &out_md5_ctx);

		printf ("input hash is:  %s\n", md5_get_str (in_hash));
		printf ("should be:      %s", md5_get_str (enc_20_md5_in));
		if (memcmp (in_hash, enc_20_md5_in, MD5_HASH_LEN) == 0)
			printf (" => OK\n\n");
		else
			printf (" => FAILURE, PRNG broken?\n\n"), exit_code++;

		printf ("output hash is: %s\n", md5_get_str (out_hash));
		printf ("should be:      %s", md5_get_str (enc_20_md5_out));
		if (memcmp (out_hash, enc_20_md5_out, MD5_HASH_LEN) == 0)
			printf (" => OK\n");
		else
			printf (" => FAILURE\n"), exit_code++;
	}

	if (test_all || (argc == 2 && !strcmp (argv[1], "80")))
	{
		printf ("\ntesting enc_type_80 with 500.000 iterations...\n");
		printf ("-------------------------------------------------------------\n");

		r250_init(1);
		MD5Init (&in_md5_ctx);
		MD5Init (&out_md5_ctx);

		for (runs=0; runs < 500000; runs++)
		{
			/* fill input with randomness */
			for (i=0; i<20; i++)
				key_80[i] = r250();	
			seed = r250();

			reverse_bytes (key_80, 20);
			MD5Update (&in_md5_ctx, (unsigned char*)key_80, 80);
			reverse_bytes (key_80, 20);

			reverse_bytes (&seed, 1);
			MD5Update (&in_md5_ctx, (unsigned char*)&seed, 4);
			reverse_bytes (&seed, 1);

			enc_type_80 (key_80, seed);

			reverse_bytes (key_80, 20);
			MD5Update (&out_md5_ctx, (unsigned char*)key_80, 80);		
			reverse_bytes (key_80, 20);
		}

		MD5Final (in_hash, &in_md5_ctx);
		MD5Final (out_hash, &out_md5_ctx);

		printf ("input hash is:  %s\n", md5_get_str (in_hash));
		printf ("should be:      %s", md5_get_str (enc_80_md5_in));
		if (memcmp (in_hash, enc_80_md5_in, MD5_HASH_LEN) == 0)
			printf (" => OK\n\n");
		else
			printf (" => FAILURE, PRNG broken?\n\n"), exit_code++;

		printf ("output hash is: %s\n", md5_get_str (out_hash));
		printf ("should be:      %s", md5_get_str (enc_80_md5_out));
		if (memcmp (out_hash, enc_80_md5_out, MD5_HASH_LEN) == 0)
			printf (" => OK\n");
		else
			printf (" => FAILURE\n"), exit_code++;
	}

	return !!exit_code;
}


