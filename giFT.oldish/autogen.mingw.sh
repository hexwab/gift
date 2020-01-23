#!/bin/sh
# $Id: autogen.mingw.sh,v 1.3 2002/08/08 00:24:19 eelcol Exp $

CC=i586-mingw32msvc-gcc \
./autogen.sh	--host=i586-mingw32msvc \
		--target=i586-mingw32msvc \
		--build=i386-linux \
		--with-voidptr=long \
		--disable-libdl \
		--with-zlib=no \
		$*
