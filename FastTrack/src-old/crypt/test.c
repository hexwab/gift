#include <stdio.h>

#include "mother.h"

void CRYPT_FUNC (unsigned int *key, unsigned int seed);

static void reverse_bytes (unsigned int *buf, unsigned int longs)
{
	unsigned char *cbuf = (unsigned char*)buf;

	for ( ; longs; longs--, buf++, cbuf += 4)
	{
		*buf = ( (unsigned int) cbuf[3] << 8 | cbuf[2]) << 16 |
			   ( (unsigned int) cbuf[1] << 8 | cbuf[0]);
	}
}

int main(void) {
	unsigned long rngseed=0x12345678;
	
	unsigned int key[20], seed;

	int count;

	(void)Mother(rngseed); /* seed it */
	for (count=0;count<1e6;count++) {
		int i;
		seed=Mother(0);
		for(i=0;i<20;i++) {
			key[i]=Mother(0);
		}
		CRYPT_FUNC (key, seed);
		reverse_bytes (key, 20);
		fwrite(key, 20, 4, stdout);
	}
	return 0;
}
