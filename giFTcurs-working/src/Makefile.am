AM_CFLAGS = -Wall

bin_PROGRAMS = giFTcurs

giFTcurs_SOURCES = \
	main.c        giftcurs.h \
	ban.c         ban.h \
	get.c         get.h \
	gift.c        gift.h \
	list.c        list.h \
	misc.c        misc.h \
	parse.c       parse.h \
	poll.c        poll.h \
	protocol.c    protocol.h \
	screen.c      screen.h \
	search.c      search.h \
	settings.c    settings.h \
	transfer.c    transfer.h \
	tree.c        tree.h \
	ui.c          ui.h \
	ui_draw.c     ui_draw.h \
	ui_help.c     ui_help.h \
	ui_main.c     ui_main.h \
	ui_mouse.c    ui_mouse.h \
	ui_settings.c ui_settings.h \
	ui_transfer.c ui_transfer.h \
	format.c      format.h \
	xcommon.c     xcommon.h

giFTcurs_LDADD = @INTLLIBS@ $(CURSES_LIBS)

EXTRA_PROGRAMS = giFTwakeup

giFTwakeup_SOURCES = \
	wakeup.c       \
	list.c        list.h \
	parse.c       parse.h \
	poll.c        poll.h \
	protocol.c    protocol.h \
	tree.c        tree.h \
	xcommon.c     xcommon.h

localedir = $(datadir)/locale
INCLUDES = -I../intl $(CURSES_INCLUDEDIR) -DLOCALEDIR=\"$(localedir)\"
