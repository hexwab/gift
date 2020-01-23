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
	ft_query.c       ft_query.h \
	ft_transfer.c    ft_transfer.h
