# $Id: Makefile.am,v 1.3 2003/09/08 05:49:11 jasta Exp $
###############################################################################

AM_CFLAGS = \
	$(OPENFT_CFLAGS)

INCLUDES = \
	-I$(top_srcdir)/src

noinst_LTLIBRARIES = libproto.la

libproto_la_SOURCES = \
	ft_handshake.c     ft_handshake.h \
	ft_sharing.c       ft_sharing.h \
	ft_stats_digest.c  ft_stats_digest.h \
	ft_query.c         ft_query.h \
	ft_push.c          ft_push.h
