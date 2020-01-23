AM_CFLAGS = $(GIFT_CFLAGS) -I$(top_srcdir)/lib

INCLUDES =

lib_LTLIBRARIES = libgiftproto.la

libgiftproto_la_SOURCES = \
	protocol.c    protocol.h \
	share.c       share.h \
	share_hash.c  share_hash.h

pkgincludedir = $(libgiftincdir)/proto
pkginclude_HEADERS = \
	protocol.h \
	share.h \
	share_hash.h \
	transfer_api.h \
	if_event_api.h
