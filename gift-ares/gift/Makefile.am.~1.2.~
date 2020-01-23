# $Id: Makefile.am,v 1.2 2004/12/04 01:31:17 mkern Exp $
AM_CFLAGS = -Wall

plugin_LTLIBRARIES = libAres.la

libAres_la_SOURCES = \
	asp_plugin.c   asp_plugin.h \
	asp_download.c asp_download.h \
	asp_hash.c     asp_hash.h \
	asp_search.c   asp_search.h \
	asp_share.c    asp_share.h \
	asp_upload.c   asp_upload.h

libAres_la_LIBADD = \
	$(top_builddir)/lib/libaresgift.la

INCLUDES = \
	@GIFT_CFLAGS@ \
	-I$(top_srcdir)/lib

libAres_la_LDFLAGS = @GIFT_LIBS@ -lm -module -avoid-version
# -Wl,-z,defs

#EXTRA_DIST = Makefile.msvc
