AM_CFLAGS = -DNDEBUG

bin_PROGRAMS = giFTcurs

if HAS_WIDE_CURSES
WIDE_TARGETS = wcwidth.c wcwidth.h
WIDE_TESTS = test_utf8
test_utf8_SOURCES = test_utf8.c parse.c list.c $(WIDE_TARGETS)
test_utf8_LDADD = @INTLLIBS@ $(GLIB_LIBS)
endif

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
	ui_input.c    ui_input.h \
	ui_main.c     ui_main.h \
	ui_mouse.c    ui_mouse.h \
	ui_settings.c \
	ui_transfer.c \
	format.c      format.h \
	xcommon.c     xcommon.h \
	$(WIDE_TARGETS)

giFTcurs_LDADD = @INTLLIBS@ $(CURSES_LIBS) $(GLIB_LIBS)

EXTRA_PROGRAMS = giFTwakeup

giFTwakeup_SOURCES = \
	wakeup.c       \
	list.c        list.h \
	parse.c       parse.h \
	protocol.c    protocol.h \
	xcommon.c     xcommon.h

giFTwakeup_LDADD = $(GLIB_LIBS)

TESTS = test_parse test_list test_gift $(WIDE_TESTS)
check_PROGRAMS = test_parse test_list test_gift $(WIDE_TESTS)
test_parse_SOURCES = test_parse.c parse.c list.c $(WIDE_TARGETS)
test_parse_LDADD = $(GLIB_LIBS)
test_list_SOURCES = test_list.c list.c
test_list_LDADD = $(GLIB_LIBS)
test_gift_SOURCES = test_gift.c gift.c list.c parse.c protocol.c $(WIDE_TARGETS)
test_gift_LDADD = @INTLLIBS@ $(GLIB_LIBS)

localedir = $(datadir)/locale
INCLUDES = -I../intl $(CURSES_INCLUDEDIR) $(GLIB_CFLAGS) -DLOCALEDIR=\"$(localedir)\"
