AM_CFLAGS = $(GIFT_CFLAGS)

INCLUDES = -I$(top_srcdir)/lib

if DAEMON
bin_PROGRAMS = giFT
else
bin_PROGRAMS =
endif

giFT_SOURCES = \
	daemon.c       daemon.h \
	plugin.c       plugin.h \
	download.c     download.h \
	if_port.c      if_port.h \
	main.c         main.h \
	md5.c          md5.h \
	mime.c         mime.h \
	sharing.c      sharing.h \
	transfer.c     transfer.h \
	upload.c       upload.h

giFT_LDADD = $(top_builddir)/lib/libgiFT.la $(GIFT_LIBS)
giFT_LDFLAGS = $(GIFT_LDFLAGS)

#giFT_DEPENDENCIES = $(top_builddir)/lib/libgiFT.la

EXTRA_DIST = 
