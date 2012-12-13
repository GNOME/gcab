AC_PREREQ(2.57)
AC_INIT([gcab], [0.0], [marcandre.lureau@gmail.com])
AC_CONFIG_SRCDIR([gcab.c])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 dist-xz no-dist-gzip tar-ustar foreign -Wall -Werror subdir-objects -Wno-portability])
AM_CONFIG_HEADER([config.h])
AM_MAINTAINER_MODE([enable])
AM_SILENT_RULES([yes])

AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL
LT_INIT([win32-dll disable-fast-install])
PKG_PROG_PKG_CONFIG
GOBJECT_INTROSPECTION_CHECK([0.9.4])

AM_PROG_VALAC([0.14])
AC_PATH_PROG(VAPIGEN, vapigen, no)
if test "x$VAPIGEN" == "xno"; then
  AC_MSG_ERROR([Cannot find the "vapigen" binary in your PATH])
fi
AC_SUBST(VAPIGEN)

GLIB_MKENUMS=`$PKG_CONFIG --variable=glib_mkenums glib-2.0`
AC_SUBST(GLIB_MKENUMS)

PKG_CHECK_MODULES(GLIB2, glib-2.0 >= 2.22 gio-2.0 >= 2.22)
AC_SUBST(GLIB2_CFLAGS)
AC_SUBST(GLIB2_LIBS)

AC_CHECK_HEADERS([zlib.h], [], [AC_MSG_ERROR([zlib required])])

AC_OUTPUT([
  Makefile
  gcab.1
  libgcab-1.0.pc
])

AC_MSG_NOTICE([
        gcab $VERSION
        ============

        prefix:                   ${prefix}
        c compiler:               ${CC}
])
