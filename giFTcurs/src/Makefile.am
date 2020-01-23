AM_CFLAGS = -Wall -Wstrict-prototypes -Wwrite-strings -Wpointer-arith -Winline

bin_PROGRAMS = giFTcurs

giFTcurs_SOURCES = \
	main.c        giftcurs.h \
	ban.c         ban.h \
	get.c         get.h \
	gift.c        gift.h \
	list.c        list.h \
	misc.c        misc.h \
	parse.c       parse.h \
	protocol.c    protocol.h \
	screen.c      screen.h \
	search.c      search.h \
	settings.c    settings.h \
	transfer.c    transfer.h \
	tree.c        tree.h \
	ui.c          ui.h \
	ui_console.c  \
	ui_draw.c     ui_draw.h \
	ui_help.c     \
	ui_main.c     ui_main.h \
	ui_mouse.c    ui_mouse.h \
	ui_settings.c \
	ui_transfer.c \
	format.c      format.h \
	xcommon.c     xcommon.h

giFTcurs_LDADD = @INTLLIBS@ $(CURSES_LIBS) $(GLIB_LIBS)

EXTRA_PROGRAMS = giFTwakeup

giFTwakeup_SOURCES = \
	wakeup.c       \
	list.c        list.h \
	parse.c       parse.h \
	protocol.c    protocol.h \
	xcommon.c     xcommon.h

giFTwakeup_LDADD = $(GLIB_LIBS)

TESTS = test_parse test_list
check_PROGRAMS = test_parse test_list
test_parse_SOURCES = test_parse.c parse.c list.c
test_parse_LDADD = $(GLIB_LIBS)
test_list_SOURCES = test_list.c list.c
test_list_LDADD = $(GLIB_LIBS)

localedir = $(datadir)/locale
INCLUDES = -I../intl $(CURSES_INCLUDEDIR) $(GLIB_CFLAGS) -DLOCALEDIR=\"$(localedir)\"
