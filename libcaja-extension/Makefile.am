CLEANFILES = $(NULL)

include $(top_srcdir)/Makefile.shared

lib_LTLIBRARIES=libcaja-extension.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBCAJA_EXTENSION_CFLAGS) \
	$(WARNING_CFLAGS) \
	$(DISABLE_DEPRECATED_CFLAGS) \
	-DDATADIR=\""$(datadir)"\" \
	$(NULL)

libcaja_extension_la_LDFLAGS=\
	-version-info @CAJA_EXTENSION_VERSION_INFO@ \
	-no-undefined \
	$(NULL)

libcaja_extension_la_LIBADD = $(LIBCAJA_EXTENSION_LIBS)

libcaja_extension_includedir=$(includedir)/caja/libcaja-extension

libcaja_extension_include_HEADERS = \
	caja-column-provider.h \
	caja-column.h \
	caja-extension-types.h \
	caja-widget-view-provider.h \
	caja-file-info.h \
	caja-info-provider.h \
	caja-location-widget-provider.h \
	caja-menu-item.h \
	caja-menu-provider.h \
	caja-property-page-provider.h \
	caja-property-page.h \
	caja-menu.h \
	caja-configurable.h \
	$(NULL)

libcaja_extension_la_SOURCES = \
	caja-column-provider.c \
	caja-column.c \
	caja-widget-view-provider.c \
	caja-extension-i18n.h \
	caja-extension-types.c \
	caja-extension-private.h \
	caja-file-info.c \
	caja-info-provider.c \
	caja-location-widget-provider.c \
	caja-menu-item.c \
	caja-menu-provider.c \
	caja-property-page-provider.c \
	caja-property-page.c \
	caja-menu.c \
	caja-configurable.c \
	$(NULL)

pkgconfigdir=$(libdir)/pkgconfig
pkgconfig_DATA=libcaja-extension.pc

EXTRA_DIST = \
	libcaja-extension.pc.in \
	$(NULL)

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir)
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

if HAVE_INTROSPECTION
introspection_files = \
	$(libcaja_extension_include_HEADERS) \
	$(libcaja_extension_la_SOURCES)
Caja-2.0.gir: libcaja-extension.la Makefile
Caja_2_0_gir_INCLUDES = Gtk-3.0 Gio-2.0 GLib-2.0
Caja_2_0_gir_CFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(LIBCAJA_EXTENSION_CFLAGS)
Caja_2_0_gir_LIBS = libcaja-extension.la
Caja_2_0_gir_FILES = $(addprefix $(srcdir)/, $(introspection_files))
INTROSPECTION_GIRS += Caja-2.0.gir

girdir = $(datadir)/gir-1.0/
gir_DATA = $(INTROSPECTION_GIRS)

typelibsdir = $(libdir)/girepository-1.0/
typelibs_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelibs_DATA)
endif

-include $(top_srcdir)/git.mk
