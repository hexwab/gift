# -*- sh -*-

# $Id: configure.ac,v 1.37 2005/02/28 20:00:25 mkern Exp $

dnl ###########################################################################
dnl Process this file with autoconf to produce a configure script.
dnl ###########################################################################

AC_PREREQ(2.52)

AC_INIT(giFT-FastTrack, 0.8.9, gift-fasttrack-devel@lists.berlios.de, giFT-FastTrack)
AC_REVISION($Revision: 1.37 $)
AC_CONFIG_SRCDIR([src/fst_fasttrack.h])

AM_INIT_AUTOMAKE
AM_CONFIG_HEADER(config.h)

dnl AM_MAINTAINER_MODE

dnl ###########################################################################
dnl Build information
dnl ###########################################################################

AC_DEFINE_UNQUOTED(BUILD_DATE, "`date`", [Build date])
AC_DEFINE(GIFT_PLUGIN, 1, [This is a giFT plugin])

dnl ###########################################################################
dnl Checks for build tools and configure libtool.
dnl ###########################################################################

AC_PROG_CC
AC_HEADER_STDC
AC_PROG_INSTALL

AM_DISABLE_STATIC
AC_PROG_LIBTOOL

dnl ###########################################################################
dnl Checks for libraries.
dnl ###########################################################################

GIFT_PLUGIN_PREFIX
GIFT_PLUGIN_PKGCONFIG(GIFT, 0.11.6, 0.12.0)

dnl ###########################################################################
dnl Checks for header files.
dnl ###########################################################################

AC_HEADER_STDC

dnl ###########################################################################
dnl Checks for typedefs, structures, and compiler characteristics.
dnl ###########################################################################

AC_C_CONST
AC_HEADER_TIME
AC_STRUCT_TM
AC_HAVE_INADDR_NONE

dnl ###########################################################################
dnl Checks for library functions.
dnl ###########################################################################

AC_FUNC_MEMCMP
AC_CHECK_FUNCS(strdup)

dnl ###########################################################################
dnl All of the lovely arguments
dnl ###########################################################################

AC_ARG_ENABLE(debug,              [  --disable-debug         do not create debug messages],,[enable_debug=yes])
AM_CONDITIONAL(DEBUG,              test x$enable_debug    = xyes)

AC_ARG_ENABLE(heavy_debug,        [  --enable-heavy-debug    create _lots_ debug messages],,[enable_heavy_debug=no])
AM_CONDITIONAL(HEAVY_DEBUG,        test x$enable_heavy_debug    = xyes)

dnl ###########################################################################
dnl normal debug
dnl ###########################################################################

use_debug=no
if test x$enable_debug = xyes; then
	AC_DEFINE(DEBUG, 1, [Enable debugging])
	use_debug=yes
fi

dnl ###########################################################################
dnl heavy debug
dnl ###########################################################################

use_heavy_debug=no
if test x$enable_heavy_debug = xyes; then
	AC_DEFINE(HEAVY_DEBUG, 1, [Enable heavy debugging])
	use_heavy_debug=yes
fi

dnl ###########################################################################
dnl Configure installation paths
dnl ###########################################################################

GIFT_PLUGIN_INSTALL_PATH

dnl ###########################################################################
dnl Finally do the generation
dnl ###########################################################################

dnl Needed to build with libtoolize 1.4.3 on Debian woody
AC_CONFIG_AUX_DIR(.)

AC_CONFIG_FILES([
  Makefile
  src/Makefile
  src/crypt/Makefile
  data/Makefile
  m4/Makefile
  win32/Makefile
  fasttrack.spec
])
AC_OUTPUT

dnl ###########################################################################
dnl Print summary to the user
dnl ###########################################################################

echo
echo "$PACKAGE $VERSION"
echo
echo "------------------ core ------------------"
echo
echo "normal debugging........: $use_debug"
echo "heavy debugging.........: $use_heavy_debug"
echo
echo "------------------------------------------"
echo
echo "$PACKAGE will be installed in $plugindir/"
echo "configs will be installed in $datadir/"
echo
