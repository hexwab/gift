#include <stdio.h>
#include "fst_crypt.h"

int main (int argc, char **argv)
{
        unsigned long seed;
	int i;
	int errors =0;

	for (i=0; i<3e5; i++)
	{
		unsigned int seed = rand(), r1, r2;
		r1 = fst_cipher_mangle_enc_type (seed, 0);
		r2 = mangle2 (seed);
		if (r1 != r2)
			exit(1);
//			errors++, printf ("%08x %08x %08x\n", seed, r1, r2);
	}
		
	return !!errors;
}
