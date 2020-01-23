# $Id: Makefile,v 1.4 2005/02/15 14:07:42 mkern Exp $
all: sniff ares_ip2port ares_login ares_nonce

sniff: sniff.c
	gcc -g -Wall -O2 sniff.c -o sniff  -lnids -lnet -lpcap -lz
