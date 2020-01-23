dnl GIFT_DEF_INT_TO_PTR
dnl
dnl Define the casting macros INT_TO_PTR and PTR_TO_INT which cast a void
dnl pointer to a proper int type (matching the sizeof of void *) so that it
dnl may be implicitly cast without a warning.
dnl
dnl Alexander Poquet <atpoquet@ucdavis.edu>

dnl Please note how m4 comments and sh comments are interspersed.  This is
dnl on purpose; sh comments will find their way into configure, where m4
dnl comments will not.

dnl GIFT_CHECK_VOIDPTR(type,action-if-false)
dnl This is a support routine, called by GIFT_DEF_INT_TO_PTR.  It compiles
dnl a test program and executes action-if-true if sizeof( long ) == 
dnl sizeof( type ).

AC_DEFUN(GIFT_CHECK_VOIDPTR,
[dnl
    # Check if sizeof( $1 ) == sizeof( void * )
    AC_TRY_RUN([int main()
        {
            if( sizeof( void * ) == sizeof( $1 ) )
                return( 0 );
            else return( 1 );
        }], [dnl

        # Yes, sizeof( void * ) == sizeof( $1 )!
        voidptr='[$1]'
        export voidptr
        AC_MSG_RESULT($1)], [$2], [dnl
        AC_MSG_ERROR([failed

*** You are cross-compiling giFT, and so giFT cannot determine what type ***
*** has the same size as void *.  Please determine this on the target    ***
*** machine and use the command line option --with-voidptr=<type> to     ***
*** configure.  (For example, --with-voidptr=long)                       ***])
        ])
])

AC_DEFUN(GIFT_DEF_INT_TO_PTR,
[dnl
    dnl Define a command line option for configure
    AC_ARG_WITH(voidptr-type, [  --with-voidptr=type     Integer type with the same length as void * (optional)],,
        with_voidptr="" )

    # GIFT_DEF_INT_TO_PTR: Determine the size of the void pointer
    if test "x$with_voidptr" = "x"
    then
        dnl Produces the checking for void * size ... configurism
        AC_MSG_CHECKING([for void * size]);

        dnl Note that it would probably be faster to compile only one
        dnl program and parse return values -- but this would be more
        dnl complicated given the limitations of autoconf and be less
        dnl portable.

        GIFT_CHECK_VOIDPTR(long, [dnl
            GIFT_CHECK_VOIDPTR(short, [dnl
                GIFT_CHECK_VOIDPTR(int, [dnl
                    GIFT_CHECK_VOIDPTR(char, [dnl
                        AC_MSG_ERROR([failed

*** giFT was unable to determine a type whose size matches the size of a ***
*** void pointer on your system.  Please determine the proper size (or a ***
*** suitable one) and pass it to configure using the --with-voidptr      ***
*** option (e.g. --with-voidptr=long)                                    ***])
                    ])
                ])
            ])
        ])
    else
        voidptr="$with_voidptr"
        export voidptr
    fi
])