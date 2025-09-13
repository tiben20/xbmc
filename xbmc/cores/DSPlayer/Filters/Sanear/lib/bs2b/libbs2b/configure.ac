#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.63])
AC_INIT([libbs2b], [3.1.0], [boris_mikhaylov@users.sourceforge.net])
AC_CONFIG_AUX_DIR([build-aux])
AM_INIT_AUTOMAKE([1.11.2 -Wall foreign subdir-objects
                  dist-zip dist-bzip2 dist-xz])
AC_CONFIG_SRCDIR([src/bs2b.h])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_LIBTOOL
PKG_PROG_PKG_CONFIG

# Checks for libraries.
PKG_CHECK_EXISTS([sndfile], [], [
    AC_MSG_ERROR(Please install libsndfile.)
])

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h malloc.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_TYPE_SIZE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([strrchr])

AC_CONFIG_FILES([libbs2b.pc
                 Makefile
                 doc/Makefile
                 src/Makefile
                 win32/Makefile])
AC_OUTPUT
