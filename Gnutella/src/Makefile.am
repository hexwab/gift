# $Id: Makefile.am,v 1.19 2004/03/26 11:44:12 hipnod Exp $
###############################################################################

AM_CFLAGS = \
	$(LIBXML2_CFLAGS) \
	$(GNUTELLA_CFLAGS)

INCLUDES = \
	-I$(top_srcdir)

SUBDIRS = \
	encoding \
	io \
	message \
	transfer

EXTRA_DIST = \
	Makefile.msvc

plugindir = $(libdir)/giFT

plugin_LTLIBRARIES = libGnutella.la

libGnutella_la_SOURCES = \
	dns.c             dns.h \
	file_cache.c      file_cache.h \
	gt_accept.c       gt_accept.h \
	gt_ban.c          gt_ban.h \
	gt_bind.c         gt_bind.h \
	gt_conf.c         gt_conf.h \
	gt_connect.c      gt_connect.h \
	gt_gnutella.c     gt_gnutella.h \
	gt_guid.c         gt_guid.h \
	gt_http_client.c  gt_http_client.h \
	gt_http_server.c  gt_http_server.h \
	gt_netorg.c       gt_netorg.h \
	gt_node_cache.c   gt_node_cache.h \
	gt_node.c         gt_node.h \
	gt_node_list.c    gt_node_list.h \
	gt_packet.c       gt_packet.h \
	gt_query_route.c  gt_query_route.h \
	gt_search.c       gt_search.h \
	gt_search_exec.c  gt_search_exec.h \
	gt_share.c        gt_share.h \
	gt_share_file.c   gt_share_file.h \
	gt_share_state.c  gt_share_state.h \
	gt_stats.c        gt_stats.h \
	gt_urn.c          gt_urn.h \
	gt_utils.c        gt_utils.h \
	gt_version.c      gt_version.h \
	gt_web_cache.c    gt_web_cache.h \
	gt_xfer.c         gt_xfer.h \
	gt_xfer_obj.c     gt_xfer_obj.h \
	http_request.c    http_request.h \
	sha1.c            sha1.h \
	trie.c            trie.h \
	xml.c             xml.h

libGnutella_la_LIBADD = \
	$(top_builddir)/src/encoding/libgt_encoding.la \
	$(top_builddir)/src/io/libgt_io.la \
	$(top_builddir)/src/message/libgt_message.la \
	$(top_builddir)/src/transfer/libgt_transfer.la \
	$(LIBXML2_LIBS) \
	$(GNUTELLA_LIBS)

libGnutella_la_LDFLAGS = \
	$(PLUGIN_LDFLAGS)
