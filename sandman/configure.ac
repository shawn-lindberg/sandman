# Autoconf uses this file to produce a configure script.
AC_INIT([sandman], [0.7])

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
AC_CHECK_LIB([mosquitto], [mosquitto_lib_init])

# Check for libxml.
PKG_CHECK_MODULES([XML], [libxml-2.0 >= 2.4])

# Checks for header files.
AC_CHECK_HEADERS([stdint.h string.h ncurses.h pigpio.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_UINT64_T

# Checks for library functions.
AC_CHECK_FUNCS([memmove strchr])

# Add an argument that can conditionally enable platform-specific features.
AC_ARG_WITH([platform], 
	[AS_HELP_STRING([--with-platform], [Allows specifying platform-specific features.])],
	[], [with_platform=rpi])
AM_CONDITIONAL([PLATFORM_RPI], [test "x$with_platform" == xrpi])

# Actually output files.
AC_OUTPUT
