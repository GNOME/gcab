AC_PREREQ(2.57)
AC_INIT([gcab], m4_esyscmd([build-aux/git-version-gen .tarball-version]),
        [https://bugzilla.gnome.org/enter_bug.cgi?product=msitools&component=gcab])
AC_CONFIG_SRCDIR([gcab.c])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.11 dist-xz no-dist-gzip tar-ustar foreign -Wall subdir-objects -Wno-portability])
AM_CONFIG_HEADER([config.h])
AM_MAINTAINER_MODE([enable])
AM_SILENT_RULES([yes])

AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_INSTALL
LT_INIT([win32-dll])
GTK_DOC_CHECK([1.14],[--flavour no-tmpl])
PKG_PROG_PKG_CONFIG
GOBJECT_INTROSPECTION_CHECK([0.9.4])
AM_CONDITIONAL([GIR], [test "x$INTROSPECTION_MAKEFILE" != x])

IT_PROG_INTLTOOL([0.40.0])
GETTEXT_PACKAGE=AC_PACKAGE_NAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE],["$GETTEXT_PACKAGE"],[The name of the gettext domain])
AM_GLIB_GNU_GETTEXT

AM_PATH_GLIB_2_0([2.22.0], [], [AC_MSG_ERROR([glib required])], [gio])
AM_PROG_VALAC([0.14])
AC_PATH_PROG(VAPIGEN, vapigen, no)
AC_SUBST(VAPIGEN)
AM_CONDITIONAL([VAPI], [test "x$VAPIGEN" != xno])
AC_CHECK_HEADERS([zlib.h], [], [AC_MSG_ERROR([zlib required])])

gl_MANYWARN_ALL_GCC([warnings])
nw=
nw="$nw -Wsystem-headers"       # Don't let system headers trigger warnings
nw="$nw -Wundef"                # All compiler preprocessors support #if UNDEF
nw="$nw -Wtraditional"          # All compilers nowadays support ANSI C
nw="$nw -Wconversion"           # These warnings usually don't point to mistakes.
nw="$nw -Wsign-conversion"      # Likewise.
nw="$nw -Wunused-parameter"     # Likewise.
nw="$nw -Wunused-local-typedefs" # Likewise.
nw="$nw -Wunsafe-loop-optimizations" # Likewise.
nw="$nw -Wbad-function-cast"    # Likewise.
# Enable all GCC warnings not in this list.
gl_MANYWARN_COMPLEMENT([warnings], [$warnings], [$nw])
for w in $warnings; do
  gl_WARN_ADD([$w])
done
gl_WARN_ADD([-Wno-sign-compare])
gl_WARN_ADD([-Wno-unused-parameter])

AM_MISSING_PROG([AUTOM4TE], [autom4te])
AC_CONFIG_TESTDIR([.])
AC_CONFIG_FILES([atlocal])
AC_CONFIG_FILES([tests/package.m4.tmp:tests/package.m4.in],
                [$srcdir/build-aux/move-if-change tests/package.m4.tmp $srcdir/tests/package.m4
                 cp $srcdir/tests/package.m4 tests/package.m4.tmp])


AC_OUTPUT([
  Makefile
  docs/Makefile
  docs/reference/Makefile
  docs/reference/gcab-docs.sgml
  gcab.1
  libgcab-1.0.pc
  libgcab/Makefile
  po/Makefile.in
])

AC_MSG_NOTICE([
        gcab $VERSION
        ============

        prefix:                   ${prefix}
        c compiler:               ${CC}
])
