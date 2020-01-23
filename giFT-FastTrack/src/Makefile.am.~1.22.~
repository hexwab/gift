# $Id: Makefile.am,v 1.22 2004/03/10 02:07:01 mkern Exp $
AM_CFLAGS = -Wall

SUBDIRS = crypt

plugin_LTLIBRARIES = libFastTrack.la

libFastTrack_la_SOURCES = \
	fst_download.c		fst_download.h \
	fst_upload.c		fst_upload.h \
	fst_fasttrack.c		fst_fasttrack.h \
	fst_hash.c			fst_hash.h \
	fst_http_header.c	fst_http_header.h \
	fst_http_client.c	fst_http_client.h \
	fst_http_server.c	fst_http_server.h \
	fst_meta.c			fst_meta.h \
	fst_node.c			fst_node.h \
	fst_packet.c		fst_packet.h \
	fst_search.c		fst_search.h \
	fst_share.c			fst_share.h \
	fst_session.c		fst_session.h \
	fst_stats.c			fst_stats.h \
	fst_utils.c			fst_utils.h \
	fst_ipset.c			fst_ipset.h \
	fst_push.c			fst_push.h \
	fst_udp_discover.c  fst_udp_discover.h \
	fst_source.c        fst_source.h \
	md5.c				md5.h

libFastTrack_la_LIBADD = \
	$(top_builddir)/src/crypt/libfst_crypt.la

INCLUDES = \
	@GIFT_CFLAGS@ \
	-I$(top_srcdir)/src/crypt

libFastTrack_la_LDFLAGS = @GIFT_LIBS@ -lm -module -avoid-version
# -Wl,-z,defs

EXTRA_DIST = Makefile.msvc
