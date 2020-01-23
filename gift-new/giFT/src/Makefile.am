AM_CFLAGS = $(GIFT_CFLAGS) $(PERL_CFLAGS)

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
	share_file.c   share_file.h \
	share_cache.c  share_cache.h \
	transfer.c     transfer.h \
	upload.c       upload.h \
	meta.c         meta.h \
	id.c           id.h \
	enum.c         enum.h \
	id_vorbis.c    id_vorbis.h \
	id_jpeg.c      id_jpeg.h \
	perl.c         perlc.h \
	hook.c         hook.h

giFT_LDADD = $(top_builddir)/lib/libgiFT.la $(GIFT_LIBS) $(PERL_LIBS)
giFT_LDFLAGS = $(GIFT_LDFLAGS) -export-dynamic

#giFT_DEPENDENCIES = $(top_builddir)/lib/libgiFT.la

EXTRA_DIST = 
