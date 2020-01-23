# $Id: Makefile.am,v 1.2 2004/12/18 15:19:56 hex Exp $
AM_CFLAGS = -Wall

noinst_LTLIBRARIES = libaresgift.la

libaresgift_la_SOURCES = \
	as_hashtable.c       as_hashtable.h \
	as_file.c            as_file.h \
	as_ares.c            as_ares.h \
	as_crypt.c           as_crypt.h \
	as_config.c          as_config.h \
	as_packet.c          as_packet.h \
	as_sha1.c            as_sha1.h \
	as_encoding.c        as_encoding.h \
	as_hash.c            as_hash.h \
	as_session.c         as_session.h \
	as_session_man.c     as_session_man.h \
	as_node.c            as_node.h \
	as_node_man.c        as_node_man.h \
	as_meta.c            as_meta.h \
	as_netinfo.c         as_netinfo.h \
	as_source.c          as_source.h \
	as_tokenize.c        as_tokenize.h \
	as_search_result.c   as_search_result.h \
	as_search.c          as_search.h \
	as_search_man.c      as_search_man.h \
	as_http_header.c     as_http_header.h \
	as_http_client.c     as_http_client.h \
	as_http_server.c     as_http_server.h \
	as_download_chunk.c  as_download_chunk.h \
	as_download_conn.c   as_download_conn.h \
	as_download_state.c  as_download_state.h \
	as_download_man.c    as_download_man.h \
	as_download.c        as_download.h \
	as_incoming.c        as_incoming.h \
	as_share.c           as_share.h \
	as_share_man.c       as_share_man.h \
	as_push.c            as_push.h \
	as_push_man.c        as_push_man.h \
	as_upload.c          as_upload.h \
	as_upload_man.c      as_upload_man.h \
	as_util.c            as_util.h \
	as_push_reply.c      as_push_reply.h \
	as_log.h

 
INCLUDES = \
	@GIFT_CFLAGS@ \
	-I$(top_srcdir)/lib

libaresgift_la_LDFLAGS = @GIFT_LIBS@ -lm -module -avoid-version
# -Wl,-z,defs

EXTRA_DIST = Makefile.msvc
