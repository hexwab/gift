# $Id: Makefile.am,v 1.4 2003/06/26 05:33:39 jasta Exp $

AM_CFLAGS = $(GIFT_CFLAGS)

INCLUDES = \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/plugin \
	-I$(top_srcdir)/OpenFT

noinst_LTLIBRARIES = libproto.la

libproto_la_SOURCES = \
	ft_handshake.c   ft_handshake.h \
	ft_sharing.c     ft_sharing.h \
	ft_stats.c       ft_stats.h \
	ft_query.c       ft_query.h \
	ft_transfer.c    ft_transfer.h
