###############################################################################
## $Id: va_copy.m4,v 1.2 2004/10/01 13:17:24 jasta Exp $
###############################################################################

dnl #
dnl # LIBGIFT_VA_COPY
dnl #
dnl # Determine the most appropriate method for copying `va_list's.  Defines
dnl # VA_COPY_FUNC or VA_COPY_BY_VAL where appropriate.
dnl #
dnl # Thanks to the GLib folks for providing this test.
dnl #

AC_DEFUN([LIBGIFT_VA_COPY],
         [AC_CACHE_CHECK([for an implementation of va_copy()],libgift_cv_va_copy,
                         [AC_LINK_IFELSE([#include <stdarg.h>
                                          void f (int i, ...) {
                                                va_list a1, a2;
                                                va_start (a1, i);
                                                va_copy (a2, a1);
                                                if (va_arg (a2, int) != 42 ||
                                                    va_arg (a1, int) != 42)
                                                        exit (1);
                                                va_end (a1);
                                                va_end (a2);
                                          }

                                          int main () {
                                                f (0, 42);
                                                return 0;
                                          }],
                                          [libgift_cv_va_copy=yes],
                                          [libgift_cv_va_copy=no])])

          AC_CACHE_CHECK([for an implementation of __va_copy()],libgift_cv___va_copy,
                         [AC_LINK_IFELSE([#include <stdarg.h>
                                          void f (int i, ...) {
                                                va_list a1, a2;
                                                va_start (a1, i);
                                                __va_copy (a2, a1);
                                                if (va_arg (a2, int) != 42 ||
                                                    va_arg (a1, int) != 42)
                                                        exit (1);
                                                va_end (a1);
                                                va_end (a2);
                                          }

                                          int main () {
                                                f (0, 42);
                                                return 0;
                                          }],
                                          [libgift_cv___va_copy=yes],
                                          [libgift_cv___va_copy=no])])

          AC_CACHE_CHECK([whether va_lists can be copied by value],libgift_cv_va_copy_by_val,
                         [AC_TRY_RUN([#include <stdarg.h>
                                      void f (int i, ...) {
                                            va_list a1, a2;
                                            va_start (a1, i);
                                            __va_copy (a2, a1);
                                            if (va_arg (a2, int) != 42 ||
                                                va_arg (a1, int) != 42)
                                                    exit (1);
                                            va_end (a1);
                                            va_end (a2);
                                      }

                                      int main () {
                                            f (0, 42);
                                            return 0;
                                      }],
                                      [libgift_cv_va_copy_by_val=yes],
                                      [libgift_cv_va_copy_by_val=no],
                                      [libgift_cv_va_copy_by_val=yes])])

          if test "x$libgift_cv_va_copy" = "xyes"; then
                LIBGIFT_DEFINE_UNQUOTED(VA_COPY_FUNC,va_copy)
          else if test "x$libgift_cv___va_copy" = "xyes"; then
                LIBGIFT_DEFINE_UNQUOTED(VA_COPY_FUNC,__va_copy)
          else if test "x$libgift_cv_copy_by_val" = "xyes"; then
                LIBGIFT_DEFINE_UNQUOTED(VA_COPY_BY_VAL)
          fi
          fi
          fi
          ])
