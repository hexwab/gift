###############################################################################
## $Id: gift-prefix.m4,v 1.1 2003/12/24 17:01:02 flexo_ Exp $
###############################################################################

dnl Fixed version of AC_PREFIX_PROGRAM from autoconf 2.57
AC_DEFUN([FIXED_AC_PREFIX_PROGRAM],
[if test "x$prefix" = xNONE; then
dnl We reimplement AC_MSG_CHECKING (mostly) to avoid the ... in the middle.
  _AS_ECHO_N([checking for prefix by ])
  AC_PATH_PROG(ac_prefix_program, [$1])
  if test -n "$ac_prefix_program"; then
    prefix=`AS_DIRNAME(["$ac_prefix_program"])`
    prefix=`AS_DIRNAME(["$prefix"])`
  fi
fi
])# FIXED_AC_PREFIX_PROGRAM

###############################################################################

AC_DEFUN([GIFT_PLUGIN_PREFIX],
  [FIXED_AC_PREFIX_PROGRAM([giftd])])

AC_DEFUN([GIFT_PLUGIN_INSTALL_PATH],
  [#
   # This is whacked.  It seems ${prefix} doesn't get set properly by
   # default and thus we cant use ${libdir} and ${datadir} without
   # overriding them. Someone please help!
   #
   if test x$prefix = xNONE; then
     prefix=`pkg-config libgift --variable=prefix`
     AC_SUBST(prefix)
   fi

   plugindir=${prefix}/lib/giFT
   AC_SUBST(plugindir)

   datadir=${prefix}/share/giFT
   AC_SUBST(datadir)
  ])
