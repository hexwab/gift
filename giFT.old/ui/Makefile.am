AM_CFLAGS = -Wall

SUBDIRS = perl

if GTK_CLIENT
bin_PROGRAMS = giFT-fe
else
bin_PROGRAMS = 
endif

INCLUDES = -I../src $(GTKCFLAGS) $(GIFT_CFLAGS)

giFT_fe_SOURCES = \
	gift-fe.h   gift-fe.c \
	ui.h        ui.c \
	search.h    search.c \
	daemon.h    daemon.c \
	feconnect.h feconnect.c \
	utils.h     utils.c \
	obj.h       obj.c \
	share.h     share.c \
	menu.h      menu.c \
	download.h  download.c \
	upload.h    upload.c \
	transfer.h  transfer.c \
	pref.h      pref.c \
	ui_utils.h  ui_utils.c \
	stats.h     stats.c
	
giFT_fe_LDADD = \
	../src/conf.o \
	../src/parse.o \
	../src/list.o \
	../src/hash.o \
	../src/dataset.o \
	../src/file.o \
	../src/network.o \
	../src/nb.o
	
giFT_fe_LDFLAGS = $(GTKLIBS) $(GIFT_LIBS)

EXTRA_DIST =
