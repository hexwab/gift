AM_CFLAGS = $(GIFT_CFLAGS)

INCLUDES =

lib_LTLIBRARIES = libgift.la

libgift_la_SOURCES = \
	libgift.h	giftconfig.h \
	array.c         array.h \
	conf.c          conf.h \
	tcpc.c          tcpc.h \
	dataset.c       dataset.h \
	event.c         event.h \
	fdbuf.c         fdbuf.h \
	file.c          file.h \
	interface.c     interface.h \
	libgift.c       libgift.h \
	list.c          list.h \
	list_lock.c     list_lock.h \
	tree.c          tree.h \
	log.c           log.h \
	memory.c        memory.h \
	network.c       network.h \
	parse.c         parse.h \
	platform.c      platform.h \
	stopwatch.c     stopwatch.h \
	strobj.c        strobj.h

pkgincludedir = $(libgiftincdir)
pkginclude_HEADERS = \
	libgift.h \
	giftconfig.h \
	array.h \
	conf.h \
	tcpc.h \
	dataset.h \
	event.h \
	fdbuf.h \
	file.h \
	interface.h \
	list.h \
	libgift.h \
	list_lock.h \
	tree.h \
	log.h \
	memory.h \
	network.h \
	parse.h \
	platform.h \
	stopwatch.h \
	strobj.h

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgift.pc

EXTRA_DIST = libgift.pc.in
DISTCLEANFILES = libgift.pc giftconfig.h
