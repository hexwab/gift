# $Id: Makefile.am,v 1.4 2004/01/07 07:24:43 hipnod Exp $

AM_CFLAGS = \
	$(LIBXML2_CFLAGS) \
	$(GNUTELLA_CFLAGS)

INCLUDES = \
	-I$(top_srcdir)/src

noinst_LTLIBRARIES = \
	libgt_message.la

libgt_message_la_SOURCES = \
	bye.c            \
	gt_message.c     \
	gt_message.h     \
	msg_handler.h    \
	ping.c           \
	ping_reply.c     \
	push.c           \
	query.c          \
	query_reply.c    \
	query_route.c    \
	vendor.c

