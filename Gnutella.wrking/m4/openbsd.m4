###############################################################################
## $Id: openbsd.m4,v 1.1 2003/07/30 08:05:44 hipnod Exp $
###############################################################################

dnl
dnl OPENBSD_LIBTOOL_WORKAROUND
dnl
dnl Work-around a problem caused by the -avoid-version libtool option.  Can
dnl anyone come up with a better way?
dnl

AC_DEFUN([OPENBSD_LIBTOOL_WORKAROUND],
         [case "${host_os}" in
            openbsd*)
              sed 's/^need_version=no$/need_version=yes/' < libtool > libtool.tmp && mv -f libtool.tmp libtool
            ;;
          esac
          ])
