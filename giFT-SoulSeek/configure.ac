# -*- sh -*-

dnl ###########################################################################
dnl Process this file with autoconf to produce a configure script.
dnl ###########################################################################

AC_PREREQ(2.52)

AC_INIT
AC_CONFIG_SRCDIR([src/sl_soulseek.h])

AM_INIT_AUTOMAKE(giFT-SoulSeek, 0.0.1)
AM_CONFIG_HEADER(config.h)

dnl ###########################################################################
dnl Checks for programs.
dnl ###########################################################################

AC_PROG_CC
AM_PROG_CC_STDC
AC_HEADER_STDC
AC_PROG_INSTALL

AM_DISABLE_STATIC
AC_PROG_LIBTOOL

# work around a bug where need_version is set to no by the above
# -avoid-version call when this is not supported on OpenBSD (or am I
# completely wrong about this?)
OPENBSD_LIBTOOL_WORKAROUND

AH_VERBATIM([BUILD_DATE], [
#undef BUILD_DATE
])
AH_VERBATIM([USE_ZLIB], [
#undef USE_ZLIB
])

dnl ###########################################################################
dnl Build information
dnl ###########################################################################

BUILD_DATE="`date`"
AC_DEFINE_UNQUOTED(BUILD_DATE, "$BUILD_DATE")

PLUGIN_LDFLAGS="-module -avoid-version"
AC_SUBST(PLUGIN_LDFLAGS)

# Override -g -O2
CFLAGS="-g -Wall"

dnl ###########################################################################
dnl Test for zlib... Copied from gift-gnutella
dnl ###########################################################################

dnl Default to compiler & linker defaults for files & libraries.
OPT_ZLIB=
AC_ARG_WITH(zlib,dnl
[  --with-zlib[=DIR]         where to look for ZLIB [compiler/linker default paths]
                        DIR points to the ZLIB installation prefix [/usr/local]],
OPT_ZLIB=$withval,
)

if test x"$OPT_ZLIB" = xno; then 
   AC_MSG_ERROR([zlib support is required])
fi

GIFT_PLUGIN_CHECK_ZLIB
GIFT_PLUGIN_PREFIX
GIFT_PLUGIN_PKGCONFIG(SL, 0.11.4, 0.12.0)
GIFT_PLUGIN_INSTALL_PATH

dnl ###########################################################################
dnl Finally do the generation
dnl ###########################################################################

AC_CONFIG_FILES([
Makefile
m4/Makefile
src/Makefile
])
AC_OUTPUT

dnl ###########################################################################
dnl Print summary to the user
dnl ###########################################################################

echo
echo "$PACKAGE $VERSION"
echo
echo "$PACKAGE will be installed in $plugindir/"
echo
echo
