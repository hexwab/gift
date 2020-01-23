# $Id: Makefile.am,v 1.6 2003/06/21 18:23:40 jasta Exp $

AM_CFLAGS = $(GIFT_CFLAGS)

INCLUDES = -I$(top_srcdir)/lib \
	   -I$(top_srcdir)/src \
	   -I$(top_srcdir)/plugin

# libdir = $(plugindir)

#
# The rest of this file is conditional on whether we're
# compiling the gnutella plugin at all
#
if GNUTELLA

plugin_LTLIBRARIES = libGnutella.la

libGnutella_la_SOURCES = \
	gt_guid.c         gt_guid.h \
	gt_accept.c       gt_accept.h \
	gt_connect.c      gt_connect.h \
	gt_conf.c         gt_conf.h \
	gt_node.c         gt_node.h \
	gt_packet.c       gt_packet.h \
	gt_protocol.c     gt_protocol.h \
	gt_netorg.c       gt_netorg.h \
	gt_utils.c        gt_utils.h \
	gt_xfer.c         gt_xfer.h \
	gt_query_route.c  gt_query_route.h \
	gt_search.c       gt_search.h \
	gt_search_exec.c  gt_search_exec.h \
	gt_share_file.c   gt_share_file.h \
	gt_share.c        gt_share.h \
	gt_stats.c        gt_stats.h \
	gt_web_cache.c    gt_web_cache.h \
	ft_http_client.c  ft_http_client.h \
	ft_http_server.c  ft_http_server.h \
	ft_xfer.c         ft_xfer.h \
	file_cache.c      file_cache.h \
	sha1.c            sha1.h \
	html.c            html.h \
	http_request.c    http_request.h \
	gt_gnutella.c     gt_gnutella.h \
	http.c            http.h

# _BIG_ hack
# libGnutella_la_LDFLAGS = -module -avoid-version -L$(top_builddir)/lib/.libs

libGnutella_la_LIBADD = \
	$(top_builddir)/lib/libgift.la \
	$(top_builddir)/plugin/libgiftproto.la

endif # GNUTELLA
