/* Where locales will be installed */
#undef LOCALEDIR

/*=== Curses version detection defines ===*/
/* Found some version of curses that we're going to use */
#undef HAS_CURSES

/* Use SunOS SysV curses? */
#undef USE_SUNOS_CURSES

/* Use old BSD curses - not used right now */
#undef USE_BSD_CURSES

/* Use SystemV curses? */
#undef USE_SYSV_CURSES

/* Use Ncurses? */
#undef USE_NCURSES

/* If you Curses does not have color define this one */
#undef NO_COLOR_CURSES

/* Define if you want to turn on SCO-specific code */
#undef SCO_FLAVOR

/* Set to reflect version of ncurses *
 *   0 = version 1.*
 *   1 = version 1.9.9g
 *   2 = version 4.0/4.1 */
#undef NCURSES_970530

/*=== End curses stuff for acconfig.h ===*/

/* va_copy support? */
#undef VA_COPY

/* Disable mouse support? */
#undef DISABLE_MOUSE

/* Don't use the internal mouse parsing code */
#undef DISABLE_INTERNAL_MOUSE

/* Defined if the resizeterm() function is available. */
#undef HAVE_RESIZETERM

/* Defined if the use_default_colors() function is available. */
#undef HAVE_USE_DEFAULT_COLORS
