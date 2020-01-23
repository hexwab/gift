###############################################################################
## $Id: gift-prefix.m4,v 1.2 2004/01/04 07:35:13 hipnod Exp $
###############################################################################

AC_DEFUN([GIFT_PLUGIN_CHECK_PREFIX],[
   libgift_prefix=`pkg-config --variable=prefix libgift`

   gift_plugin_prefix=$prefix
   if test x"$prefix" = xNONE; then
      gift_plugin_prefix=/usr/local
   fi

   if test x"$libgift_prefix" != x"$gift_plugin_prefix"; then
      AC_MSG_WARN([
   You are trying to install in $gift_plugin_prefix, but I only
   detected a giFT installation in $libgift_prefix. 
   You may be installing in the wrong place.

   You should probably supply --prefix=$libgift_prefix 
   to configure. Or, if you have a giFT installation in
   $gift_plugin_prefix, you could add ${gift_plugin_prefix}/lib/pkgconfig 
   to the PKG_CONFIG_PATH environment variable, so I can detect it.
])
   fi
])
