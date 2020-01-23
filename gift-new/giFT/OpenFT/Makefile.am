AM_CFLAGS = $(GIFT_CFLAGS)

INCLUDES = -I$(top_srcdir)/lib -I$(top_srcdir)/src

plugin_LTLIBRARIES = libOpenFT.la

libOpenFT_la_SOURCES = \
	openft.c      openft.h \
	node.c        node.h \
	packet.c      packet.h \
	protocol.c    protocol.h \
	utils.c       utils.h \
	daemon.c      daemon.h \
	share.c       share.h \
	search.c      search.h \
	html.c        html.h \
	netorg.c      netorg.h \
	share_db.c    share_db.h \
	share_comp.c  share_comp.h \
	xfer.c        xfer.h \
	http_client.c http_client.h \
	http_server.c http_server.h

libOpenFT_la_LIBADD = $(top_builddir)/lib/libgiFT.la

# _BIG_ hack
libOpenFT_la_LDFLAGS = -module -avoid-version -L$(top_builddir)/lib/.libs
