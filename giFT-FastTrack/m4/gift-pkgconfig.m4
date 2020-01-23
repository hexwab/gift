###############################################################################
## $Id: gift-pkgconfig.m4,v 1.2 2004/04/14 21:06:23 mkern Exp $
###############################################################################

AC_DEFUN([GIFT_PLUGIN_PKGCONFIG],
  [AC_PATH_PROG(PKG_CONFIG, pkg-config)

   if test x$prefix != xNONE;  then
     PKG_CONFIG_PATH="$PKG_CONFIG_PATH:$prefix/lib/pkgconfig"
   fi

   PKG_CONFIG_PATH="$PKG_CONFIG_PATH:/usr/local/lib/pkgconfig"
   export PKG_CONFIG_PATH

   PKG_CHECK_MODULES([$1], libgift >= $2 libgift < $3)

   # fudge libgiftproto in there which doesnt have any pkg-config entry
   AS_VAR_SET($1_LIBS, "AS_VAR_GET($1_LIBS) -lgiftproto")

   # hack to set libgift_version
   libgift_version=`pkg-config libgift --modversion`

   AC_SUBST($1_CFLAGS)
   AC_SUBST($1_LIBS)
  ])
