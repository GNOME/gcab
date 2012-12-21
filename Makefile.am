NULL =
BUILT_SOURCES =
CLEANFILES =
DISTCLEANFILES =
ACLOCAL_AMFLAGS = -I m4
AM_MAKEFLAGS = --no-print-directory

AM_CPPFLAGS =					\
        -I$(top_srcdir)				\
        -DG_LOG_DOMAIN=\"gcab\"			\
	$(NULL)

AM_LDFLAGS = -lz $(GLIB2_LIBS)
AM_CFLAGS = $(GLIB2_CFLAGS)

libgcab_includedir = $(includedir)/libgcab-1.0
libgcab_priv_includedir = $(libgcab_includedir)/libgcab
libgcab_include_HEADERS = libgcab.h
libgcab_priv_include_HEADERS =			\
	libgcab/gcab-file.h			\
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
	$(NULL)
libgcab_1_0_la_LDFLAGS =				\
	-version-info 0:0:0				\
	-export-symbols-regex='^gcab_'			\
	-Wl,--version-script=${srcdir}/libgcab.syms	\
	-no-undefined					\
	$(NULL)

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libgcab-1.0.pc

bin_PROGRAMS = gcab

gcab_SOURCES =					\
	gcab.c					\
	$(NULL)
gcab_LDADD = libgcab-1.0.la

GCAB_ENUMS =					\
	libgcab/gcab-enums.c			\
	libgcab/gcab-enums.h			\
	$(NULL)

BUILT_SOURCES += $(GCAB_ENUMS)

ENUMS_FILES =					\
	libgcab/gcab-folder.h			\
	$(NULL)

$(GCAB_ENUMS): %: %.etemplate $(ENUMS_FILES)
	$(AM_V_GEN)$(GLIB_MKENUMS) --template $^ | sed -e 's/g_cab/gcab/g' | sed -e 's/G_CAB/GCAB/g' | sed -e 's/_CAB//g' > $@

EXTRA_DIST =					\
	MAINTAINERS				\
	gcab.doap				\
	libgcab.syms				\
	$(BUILT_SOURCES)			\
	libgcab/gcab-enums.c.etemplate		\
	libgcab/gcab-enums.h.etemplate		\
	$(NULL)

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

# Autotest support

TESTSUITE = $(srcdir)/tests/testsuite
EXTRA_DIST += tests/testsuite.at $(srcdir)/tests/package.m4 $(TESTSUITE)
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

MAINTAINERCLEANFILES =						\
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
	$(srcdir)/install-sh					\
	$(srcdir)/ltmain.sh					\
	$(srcdir)/missing					\
	$(srcdir)/mkinstalldirs					\
	`find "$(srcdir)" -type f -name Makefile.in -print`	\
	$(NULL)

-include $(top_srcdir)/git.mk
