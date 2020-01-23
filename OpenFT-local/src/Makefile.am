# $Id: Makefile.am,v 1.7 2004/09/03 16:42:59 hexwab Exp $
###############################################################################

SUBDIRS = proto

# shouldn't ZLIB_CFLAGS be here...?
AM_CFLAGS = \
	$(OPENFT_CFLAGS) \
	$(LIBDB_CFLAGS)

INCLUDES = \
	-I$(top_srcdir)/src

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
	ft_bloom.c       ft_bloom.h \
	ft_share.c       ft_share.h \
	ft_share_file.c  ft_share_file.h \
	ft_stats.c       ft_stats.h \
	ft_search.c      ft_search.h \
	ft_search_db.c   ft_search_db.h \
	ft_routing.c     ft_routing.h \
	ft_search_exec.c ft_search_exec.h \
	ft_tokenize.c    ft_tokenize.h \
	ft_search_obj.c  ft_search_obj.h \
	ft_netorg.c      ft_netorg.h \
	ft_transfer.c    ft_transfer.h \
	ft_http.c        ft_http.h \
	ft_http_client.c ft_http_client.h \
	ft_http_server.c ft_http_server.h \
	ft_stream.c      ft_stream.h \
	ft_session.c     ft_session.h \
	md5.c            md5.h

libOpenFT_la_LIBADD = \
	$(top_builddir)/src/proto/libproto.la \
	$(OPENFT_LIBS) \
	$(LIBDB_LIBS)

libOpenFT_la_LDFLAGS = \
	$(PLUGIN_LDFLAGS)

EXTRA_DIST = Makefile.msvc
