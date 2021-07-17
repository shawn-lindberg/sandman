# Autoconf uses this file to produce a configure script.
AC_INIT([sandman], [0.5])

AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_CONFIG_MACRO_DIR([m4])

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([Makefile source/Makefile data/Makefile])

# Use libtool, but only static linking.
LT_INIT([disable-shared])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC

# Check for pkg-config.
PKG_PROG_PKG_CONFIG

# Checks for libraries.
AC_CHECK_LIB([rt], [clock_gettime])
AC_CHECK_LIB([ncurses], [initscr])
AC_CHECK_LIB([pigpio], [gpioInitialise])

# Check for SDL.
PKG_CHECK_MODULES([SDL_MIXER], [SDL_mixer])

# Check for libxml.
PKG_CHECK_MODULES([XML], [libxml-2.0 >= 2.4])

# Checks for header files.
AC_CHECK_HEADERS([stdint.h string.h ncurses.h pigpio.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_UINT64_T

# Checks for library functions.
AC_CHECK_FUNCS([memmove strchr])

# Add an argument that can conditionally enable Sphinx speech 
# recognition.
AC_ARG_WITH([sphinx], 
	[AS_HELP_STRING([--with-sphinx], [Enable Sphinx speech recognition])],
	[], [with_sphinx=no])
AM_CONDITIONAL([SPHINX_SPEECH_RECOGNITION], [test "x$with_sphinx" != xno])

# Actually output files.
AC_OUTPUT
