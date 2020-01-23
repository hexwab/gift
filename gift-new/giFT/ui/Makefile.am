AM_CFLAGS = -Wall

SUBDIRS = perl

if GTK_CLIENT
bin_PROGRAMS = giFT-fe
else
bin_PROGRAMS = 
endif

INCLUDES = -I$(top_srcdir)/lib -I$(top_srcdir)/src -Ipixmaps/ $(GTKCFLAGS) $(GIFT_CFLAGS)

giFT_fe_SOURCES = \
	fe_daemon.h     fe_daemon.c \
	fe_download.h   fe_download.c \
	fe_share.h      fe_share.c \
	fe_transfer.h   fe_transfer.c \
	fe_connect.h    fe_connect.c \
	fe_fifo.h       fe_fifo.c \
	fe_menu.h       fe_menu.c \
	fe_obj.h        fe_obj.c \
	fe_pref.h       fe_pref.c \
	fe_search.h     fe_search.c \
	fe_stats.h      fe_stats.c \
	fe_ui.h         fe_ui.c \
	fe_ui_utils.h   fe_ui_utils.c \
	fe_upload.h     fe_upload.c \
	fe_utils.h      fe_utils.c \
	gift-fe.h       gift-fe.c
	
giFT_fe_LDADD = $(top_builddir)/lib/libgiFT.la $(GIFT_LIBS)
giFT_fe_LDFLAGS = $(GTKLIBS) $(GIFT_LIBS)

EXTRA_DIST =
