# $Id: Makefile.am,v 1.72 2003/06/27 10:05:04 jasta Exp $

SUBDIRS = meta

AM_CFLAGS = \
	$(GIFT_CFLAGS) \
	$(PERL_CFLAGS) \
	$(ID3LIB_CFLAGS) \
	$(VORBIS_CFLAGS) \
	$(IMAGEMAGICK_CFLAGS)

if LTDL
incadd=
else
incadd=-I$(top_srcdir)/OpenFT -I$(top_srcdir)/Gnutella
endif

INCLUDES = $(incadd) \
	-I$(top_srcdir)/lib \
	-I$(top_srcdir)/plugin

if DAEMON
bin_PROGRAMS = giFT
else
bin_PROGRAMS =
endif

giFT_SOURCES = \
	daemon.c          daemon.h \
	plugin.c          plugin.h \
	download.c        download.h \
	download_state.c  download_state.h \
	if_event.c        if_event.h \
	if_port.c         if_port.h \
	if_message.c      if_message.h \
	if_search.c       if_search.h \
	if_share.c        if_share.h \
	if_stats.c        if_stats.h \
	if_transfer.c     if_transfer.h \
	giftd.c           giftd.h \
	mime.c            mime.h \
	meta.c            meta.h \
	opt.c             opt.h \
	share_cache.c     share_cache.h \
	share_db.c        share_db.h \
	share_local.c     share_local.h \
	transfer.c        transfer.h \
	upload.c          upload.h

giFT_LDADD = \
	$(top_builddir)/lib/libgift.la \
	$(top_builddir)/plugin/libgiftproto.la \
	$(top_builddir)/src/meta/libmeta.la \
	$(GIFT_LIBS) \
	$(PERL_LIBS) \
	$(ID3LIB_LIBS) \
	$(VORBISFILE_LIBS) \
	$(VORBIS_LIBS) \
	$(IMAGEMAGICK_LIBS)

giFT_LDFLAGS = \
	$(GIFT_LDFLAGS) \
	$(IMAGEMAGICK_LDFLAGS) \
	-export-dynamic

EXTRA_DIST =
