
CRYPTSRCS=../src/crypt/enc_type_*.lo ../src/crypt/fst_crypt.c ../src/md5.c

HASHERSRCS=../src/fst_hash.c ../src/fst_utils.c ../src/md5.c

CFLAGS=-g -O2 -Wall -I../src/crypt -I../src


all: xor filter sniff kzf2nodes hasher

sniff: sniff.c
	$(CC) $(CFLAGS) $(CRYPTSRCS) sniff.c -o sniff -lnids -lnet -lpcap 

hasher: 
	$(CC) $(CFLAGS) -DHASH_TEST $(HASHERSRCS) -o hasher

clean:
	rm -f xor filter sniff kzf2nodes hasher

