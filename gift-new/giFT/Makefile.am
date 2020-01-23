SUBDIRS = lib OpenFT src ui data etc

man_MANS = giFT.1 gift.conf.5 OpenFT.conf.5

EXTRA_DIST = giFT-setup $(man_MANS)

setupdir = $(prefix)/bin
setup_SCRIPTS = giFT-setup

# TODO -- get giftconfig.h written directly to lib/
pkginclude_HEADERS = \
	giftconfig.h
