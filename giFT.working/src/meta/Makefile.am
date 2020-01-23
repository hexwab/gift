# $Id: Makefile.am,v 1.9 2004/04/12 22:21:52 mkern Exp $
###############################################################################

SUBDIRS =

AM_CFLAGS = \
	$(GIFT_CFLAGS) \
	$(PERL_CFLAGS) \
	$(VORBIS_CFLAGS) \
	$(IMAGEMAGICK_CFLAGS)

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_srcdir)/src

noinst_LTLIBRARIES = libmeta.la

libmeta_la_SOURCES = \
	meta_avi.c      meta_avi.h \
	meta_image.c    meta_image.h \
	meta_mp3.c      meta_mp3.h \
	meta_mpeg.c     meta_mpeg.h \
	meta_ogg.c      meta_ogg.h \
	meta_tag.c      meta_tag.h \
	meta_torrent.c  meta_torrent.h

libmeta_la_LDFLAGS = \
	$(GIFT_LDFLAGS)

libmeta_la_LIBADD = \
	$(GIFT_LIBS)
