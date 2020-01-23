#include <stdio.h>
#include "crypt/fst_crypt.h"

int main (int argc, char **argv)
{
	FSTCipher c;
	unsigned long enc_type, seed;
	int ch;

	if (argc<2) {
		fprintf(stderr, "Usage: %s enc_type seed\n", *argv);
		exit(2);
	}

	enc_type=strtoul(argv[1], NULL, 16);
	seed=strtoul(argv[2], NULL, 16);

	fprintf(stderr, "%lx %lx\n", enc_type, seed);

	if (!fst_cipher_init(&c, seed, enc_type))
		exit(1);
	
	while ((ch=getchar())!=EOF)
		putchar(ch ^ fst_cipher_clock (&c));

	exit(0);
}
