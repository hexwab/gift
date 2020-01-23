# $Id: Makefile.am,v 1.14 2004/04/14 21:06:23 mkern Exp $
AM_CFLAGS = -Wall

noinst_LTLIBRARIES = libfst_crypt.la

libfst_crypt_la_SOURCES = \
	enc_type_1.c \
	enc_type_2.c \
	enc_type_20.c \
	enc_type_80.c \
	fst_crypt.c fst_crypt.h
 
INCLUDES = \
	-I$(top_srcdir)/src

TESTS = crypt_test
check_PROGRAMS = crypt_test
crypt_test_SOURCES = crypt_test.c
crypt_test_LDADD = ../md5.lo enc_type_1.lo enc_type_2.lo enc_type_20.lo enc_type_80.lo
