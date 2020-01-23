# $Id: Makefile.am,v 1.43 2003/06/24 20:23:29 jasta Exp $

SUBDIRS = proto

AM_CFLAGS = $(GIFT_CFLAGS) $(LIBDB_CFLAGS)

INCLUDES = \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/src \
	-I$(top_srcdir)/plugin \
	$(LIBDB_CFLAGS)

plugin_LTLIBRARIES = libOpenFT.la

libOpenFT_la_SOURCES = \
	ft_openft.c      ft_openft.h \
	ft_node.c        ft_node.h \
	ft_node_cache.c  ft_node_cache.h \
	ft_conf.c        ft_conf.h \
	ft_conn.c        ft_conn.h \
	ft_guid.c        ft_guid.h \
	ft_packet.c      ft_packet.h \
	ft_protocol.c    ft_protocol.h \
	ft_utils.c       ft_utils.h \
	ft_version.c     ft_version.h \
	ft_share.c       ft_share.h \
	ft_share_file.c  ft_share_file.h \
	ft_stats.c       ft_stats.h \
	ft_search.c      ft_search.h \
	ft_search_db.c   ft_search_db.h \
	ft_search_exec.c ft_search_exec.h \
	ft_search_obj.c  ft_search_obj.h \
	ft_html.c        ft_html.h \
	ft_netorg.c      ft_netorg.h \
	ft_xfer.c        ft_xfer.h \
	ft_http_client.c ft_http_client.h \
	ft_http_server.c ft_http_server.h \
	ft_stream.c      ft_stream.h \
	ft_session.c     ft_session.h \
	md5.c            md5.h

libOpenFT_la_LIBADD = \
	$(top_builddir)/lib/libgift.la \
	$(top_builddir)/plugin/libgiftproto.la \
	$(top_builddir)/OpenFT/proto/libproto.la \
	$(LIBDB_LIBS)

# _BIG_ hack
# libOpenFT_la_LDFLAGS = -module -avoid-version -L$(top_builddir)/lib/.libs
