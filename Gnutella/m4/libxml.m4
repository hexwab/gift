###############################################################################
## $Id: libxml.m4,v 1.1 2003/09/17 23:25:51 hipnod Exp $
###############################################################################

AC_DEFUN([GIFT_GNUTELLA_CHECK_LIBXML],[
   #
   # Check on the user's PATH if no path was specified in OPT_LIBXML,
   # when the user has passed --with-libxml without a directory,
   # or check the directory if one was supplied.
   #
   if test x"$OPT_LIBXML" = xyes; then 
      AC_PATH_PROG(XML2_CONFIG, xml2-config, [no])
   else
      XMLPATH="$OPT_LIBXML/bin"
      AC_PATH_PROG(XML2_CONFIG, xml2-config, [no], [$XMLPATH])
   fi

   if test x"$XML2_CONFIG" != xno; then
      LIBXML2_CFLAGS=`$XML2_CONFIG --cflags`
      LIBXML2_LIBS=`$XML2_CONFIG --libs`

      AC_SUBST(LIBXML2_CFLAGS) 
      AC_SUBST(LIBXML2_LIBS) 
      AC_DEFINE(USE_LIBXML2) 
   else
      AC_MSG_ERROR([
Couldn't run ${OPT_LIBXML}/bin/xml2-config

])
   fi
])
