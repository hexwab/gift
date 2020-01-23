AC_INIT(src/opn_opennap.c)
AM_INIT_AUTOMAKE(giFT-OpenNap, 0.0.1)

AM_CONFIG_HEADER(config.h)

AH_TEMPLATE([OPENNAP_DEBUG], [Define to 1 if you want debugging output])
AH_TEMPLATE([DATADIR], [Define to the directory where the giFT-OpenNap data files go])
AH_VERBATIM([_GNU_SOURCE], [/* Enable GNU extensions on systems that have them. */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif
])

AC_PROG_CC
AM_DISABLE_STATIC
AC_PROG_LIBTOOL
 
AC_ARG_ENABLE(debug,
		[  --enable-debug          debug build (default: no)], [
			AC_DEFINE(OPENNAP_DEBUG)
			CFLAGS=`echo "$CFLAGS -g -Wall" | sed s/" -s"//`
		])

PKG_CHECK_MODULES(GIFT_OPENNAP, libgift >= 0.10.0)

if test x$prefix = xNONE; then
    prefix=`pkg-config libgift --variable=prefix`
fi

plugindir=${prefix}/lib/giFT
AC_SUBST(plugindir)

AC_EXPAND_DIR(DATADIR, "$datadir/giFT/OpenNap")
AC_DEFINE_UNQUOTED(DATADIR, "$DATADIR")
AC_SUBST(DATADIR)

AC_CHECK_FUNCS(strcasestr)

AC_OUTPUT([
Makefile
data/Makefile
src/Makefile
])

