#!/bin/sh
# $Id: autogen.mingw.sh,v 1.2 2002/05/08 23:21:35 rossta Exp $

CC=i586-mingw32msvc-gcc \
./autogen.sh	--host=i586-mingw32msvc \
		--target=i586-mingw32msvc \
		--build=i386-linux \
		--with-voidptr=long \
		--disable-gtk-client \
		--disable-libdl \
		--with-zlib=no \
		$*
