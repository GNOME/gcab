NULL =
BUILT_SOURCES =
CLEANFILES =
DISTCLEANFILES =
ACLOCAL_AMFLAGS = -I m4

makeflags_ = $(makeflags_$(AM_DEFAULT_VERBOSITY))
makeflags_0 = --no-print-directory -s
makeflags_1 =
AM_MAKEFLAGS = $(makeflags_$(V))

SUBDIRS = . po libgcab docs

AM_CFLAGS = $(WARN_CFLAGS)
AM_CPPFLAGS =					\
        -I$(top_srcdir)				\
        -DG_LOG_DOMAIN=\"gcab\"			\
	-DLOCALEDIR="\"$(datadir)/locale\""	\
	$(GLIB_CFLAGS)				\
	$(NULL)

libgcab_includedir = $(includedir)/libgcab-1.0
libgcab_priv_includedir = $(libgcab_includedir)/libgcab
libgcab_include_HEADERS = libgcab.h
libgcab_priv_include_HEADERS =			\
	libgcab/gcab-cabinet.h			\
	libgcab/gcab-enums.h			\
	libgcab/gcab-file.h			\
	libgcab/gcab-folder.h			\
	$(NULL)

lib_LTLIBRARIES = libgcab-1.0.la
libgcab_1_0_la_SOURCES =			\
	libgcab/gcab-enums.c			\
	libgcab/gcab-cabinet.c			\
	libgcab/gcab-cabinet.h			\
	libgcab/gcab-folder.c			\
	libgcab/gcab-folder.h			\
	libgcab/gcab-file.c			\
	libgcab/gcab-file.h			\
	libgcab/gcab-priv.h			\
	libgcab/cabinet.c			\
	libgcab/cabinet.h			\
	libgcab/decomp.c			\
	libgcab/decomp.h			\
	libgcab/glib-compat.c			\
	libgcab/glib-compat.h			\
	$(NULL)
libgcab_1_0_la_CPPFLAGS =				\
	$(AM_CPPFLAGS)					\
	-DGETTEXT_PACKAGE="\"$(GETTEXT_PACKAGE)\""	\
	$(NULL)
libgcab_1_0_la_LIBADD = -lz $(GLIB_LIBS)
libgcab_1_0_la_LDFLAGS =				\
	-version-info 0:0:0				\
	-Wl,--version-script=${srcdir}/libgcab.syms	\
	-no-undefined					\
	$(NULL)
libgcab_1_0_la_DEPENDENCIES = libgcab.syms

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgcab-1.0.pc

man1_MANS = gcab.1

bin_PROGRAMS = gcab

gcab_SOURCES =					\
	gcab.c					\
	$(NULL)
gcab_LDADD =                                    \
	$(GLIB_LIBS)                            \
	libgcab-1.0.la                          \
	$(NULL)

GCAB_ENUMS =					\
	libgcab/gcab-enums.c			\
	libgcab/gcab-enums.h			\
	$(NULL)

BUILT_SOURCES += $(GCAB_ENUMS)

ENUMS_FILES =					\
	libgcab/gcab-folder.h			\
	libgcab/gcab-file.h			\
	$(NULL)

$(GCAB_ENUMS): %: %.etemplate $(ENUMS_FILES)
	$(AM_V_GEN)$(GLIB_MKENUMS) --template $^ | sed -e 's/g_cab/gcab/g' | sed -e 's/G_CAB/GCAB/g' | sed -e 's/_CAB//g' > $@

EXTRA_DIST =					\
	gcab.doap				\
	libgcab.syms				\
	$(BUILT_SOURCES)			\
	libgcab/gcab-enums.c.etemplate		\
	libgcab/gcab-enums.h.etemplate		\
	build-aux/git-version-gen		\
	$(NULL)

if GIR
INTROSPECTION_SCANNER_ARGS = --warn-all

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS = GCab-1.0.gir

GCab-1.0.gir: libgcab-1.0.la
GCab_1_0_gir_NAMESPACE = GCab
GCab_1_0_gir_VERSION = 1.0
GCab_1_0_gir_CFLAGS = $(AM_CPPFLAGS)
GCab_1_0_gir_INCLUDES = GObject-2.0 GLib-2.0 Gio-2.0
GCab_1_0_gir_LIBS = libgcab-1.0.la
GCab_1_0_gir_FILES =				\
	$(top_srcdir)/libgcab.h			\
	$(top_srcdir)/libgcab/gcab-cabinet.h	\
	$(top_srcdir)/libgcab/gcab-enums.h	\
	$(top_srcdir)/libgcab/gcab-file.h	\
	$(top_srcdir)/libgcab/gcab-folder.h	\
	$(top_srcdir)/libgcab/gcab-cabinet.c	\
	$(top_srcdir)/libgcab/gcab-enums.c	\
	$(top_srcdir)/libgcab/gcab-file.c	\
	$(top_srcdir)/libgcab/gcab-folder.c	\
	$(NULL)
GCab_1_0_gir_EXPORT_PACKAGES = libgcab-1.0
GCab_1_0_gir_SCANNERFLAGS =			\
	--identifier-prefix=GCab		\
	--symbol-prefix=gcab			\
	--c-include="libgcab.h"			\
	$(NULL)

girdir = $(datadir)/gir-1.0
gir_DATA = GCab-1.0.gir
typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelib_DATA)

if VAPI
vapidir = $(datadir)/vala/vapi
vapi_DATA = libgcab-1.0.vapi

BUILT_SOURCES += $(vapi_DATA)

libgcab-1.0.vapi: GCab-1.0.gir
	$(AM_V_GEN)$(VAPIGEN) -q		\
		--metadatadir=$(srcdir)		\
		--vapidir=$(builddir)		\
		--library libgcab-1.0		\
		--pkg gio-2.0			\
		$<

CLEANFILES += libgcab-1.0.vapi
endif # VAPI
endif # GIR

# Autotest support

TESTSUITE = $(srcdir)/tests/testsuite
EXTRA_DIST += tests/testsuite.at $(srcdir)/tests/package.m4 $(TESTSUITE)
EXTRA_DIST += build-aux/move-if-change

DISTCLEANFILES += atconfig
CLEANFILES += testsuite.log

check-local: $(TESTSUITE) atconfig
	builddir=`pwd` && \
		$(SHELL) $(TESTSUITE) AUTOTEST_PATH=$$builddir $(TESTSUITEFLAGS)

installcheck-local: $(TESTSUITE) atconfig
	$(SHELL) $(TESTSUITE) AUTOTEST_PATH=$(bindir) $(TESTSUITEFLAGS)

clean-local:
	-$(SHELL) $(TESTSUITE) --clean

$(TESTSUITE): tests/testsuite.at tests/package.m4
	$(AUTOM4TE) --language=autotest -I$(builddir)/tests -o $@.tmp $@.at
	mv $@.tmp $@

DISTCHECK_CONFIGURE_FLAGS = --enable-gtk-doc

MAINTAINERCLEANFILES =						\
	$(srcdir)/ABOUT-NLS					\
	$(srcdir)/INSTALL					\
	$(srcdir)/aclocal.m4					\
	$(srcdir)/autoscan.log					\
	$(srcdir)/build-aux					\
	$(srcdir)/compile					\
	$(srcdir)/config.guess					\
	$(srcdir)/config.h.in					\
	$(srcdir)/config.sub					\
	$(srcdir)/configure.scan				\
	$(srcdir)/depcomp					\
	$(srcdir)/gtk-doc.make					\
	$(srcdir)/install-sh					\
	$(srcdir)/ltmain.sh					\
	$(srcdir)/m4/*.m4					\
	$(srcdir)/missing					\
	$(srcdir)/mkinstalldirs					\
	$(srcdir)/po/Makevars.template				\
	$(srcdir)/po/Rules-quot					\
	$(srcdir)/po/boldquot.sed				\
	$(srcdir)/po/en@boldquot.header				\
	$(srcdir)/po/en@quot.header				\
	$(srcdir)/po/insert-header.sin				\
	$(srcdir)/po/quot.sed					\
	$(srcdir)/po/remove-potcdate.sin			\
	$(srcdir)/tests/package.m4				\
	$(srcdir)/tests/testsuite				\
	`find "$(srcdir)" -type f -name Makefile.in -print`	\
	$(NULL)

EXTRA_DIST += $(top_srcdir)/.version
BUILT_SOURCES += $(top_srcdir)/.version
$(top_srcdir)/.version:
	echo $(VERSION) > $@-t && mv $@-t $@
dist-hook:
	echo $(VERSION) > $(distdir)/.tarball-version

-include $(top_srcdir)/git.mk
