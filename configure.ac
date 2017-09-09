#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])

AC_INIT([toymon], [0.1.0], [aggy_lzzy@yahoo.co.jp])

AC_CONFIG_SRCDIR([src/tm_main.c])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_HEADERS([config.h])

AM_INIT_AUTOMAKE([-Wall -Werror dist-xz no-dist-gzip])

AM_SILENT_RULES([yes])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
PKG_CHECK_MODULES([XCB_SHAPE], [xcb-shape])
PKG_CHECK_MODULES([CAIRO_XCB], [cairo-xcb])
PKG_CHECK_MODULES([PANGOCAIRO], [pangocairo])
PKG_CHECK_MODULES([LIBRSVG], [librsvg-2.0])

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.

AC_CONFIG_FILES([Makefile
                 src/Makefile
		 data/Makefile])
AC_OUTPUT
