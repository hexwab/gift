AM_CFLAGS = \
	$(GIFT_CFLAGS) \
	$(PERL_CFLAGS) \
	$(ID3LIB_CFLAGS) \
	$(VORBIS_CFLAGS) \
	$(IMAGEMAGICK_CFLAGS)

if LIBDL
incadd=
else
incadd=-I$(top_srcdir)/OpenFT
endif

INCLUDES = -I$(top_srcdir)/lib $(incadd)

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
	if_message.c   if_message.h \
	if_search.c    if_search.h \
	if_share.c     if_share.h \
	if_stats.c     if_stats.h \
	if_transfer.c  if_transfer.h \
	main.c         main.h \
	md5.c          md5.h \
	mime.c         mime.h \
	meta.c         meta.h \
	meta_mp3.c     meta_mp3.h \
	meta_ogg.c     meta_ogg.h \
	meta_image.c   meta_image.h \
	share_file.c   share_file.h \
	share_cache.c  share_cache.h \
	share_db.c     share_db.h \
	share_hash.c   share_hash.h \
	transfer.c     transfer.h \
	upload.c       upload.h \
	perl.c         perlc.h \
	hook.c         hook.h

giFT_LDADD = \
	$(top_builddir)/lib/libgiFT.la \
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

#giFT_DEPENDENCIES = $(top_builddir)/lib/libgiFT.la

EXTRA_DIST =
