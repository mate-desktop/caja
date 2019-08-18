include $(top_srcdir)/Makefile.shared

SUBDIRS=file-manager

bin_PROGRAMS = \
	caja \
	caja-file-management-properties \
	caja-autorun-software \
	caja-connect-server \
	$(NULL)

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir)/libcaja-private \
	$(CORE_CFLAGS) \
	$(WARNING_CFLAGS) \
	$(EXIF_CFLAGS) \
	$(EXEMPI_CFLAGS) \
	-DDATADIR=\""$(datadir)"\" \
	-DLIBDIR=\""$(libdir)"\" \
	-DCAJA_DATADIR=\""$(datadir)/caja"\" \
	-DMATELOCALEDIR=\""$(datadir)/locale"\" \
	-DUIDIR=\""$(datadir)/caja/ui"\" \
	-DCAJA_PIXMAPDIR=\""$(datadir)/pixmaps/caja"\" \
	-DPREFIX=\""$(prefix)"\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DVERSION="\"$(VERSION)\"" \
	$(DISABLE_DEPRECATED_CFLAGS) \
	$(NULL)

LDADD = \
	$(top_builddir)/src/file-manager/libcaja-file-manager.la \
	$(top_builddir)/libcaja-private/libcaja-private.la \
	$(CORE_LIBS) \
	$(EXIF_LIBS) \
	$(EXEMPI_LIBS) \
	$(POPT_LIBS) \
    -lnotify
	$(NULL)

dbus_freedesktop_built_sources =			\
	caja-freedesktop-generated.c		\
	caja-freedesktop-generated.h

$(dbus_freedesktop_built_sources) : Makefile.am $(top_srcdir)/data/freedesktop-dbus-interfaces.xml
	gdbus-codegen									\
		--interface-prefix org.freedesktop.					\
		--c-namespace CajaFreedesktop					\
		--c-generate-object-manager						\
		--generate-c-code caja-freedesktop-generated			\
		$(top_srcdir)/data/freedesktop-dbus-interfaces.xml			\
		$(NULL)

BUILT_SOURCES = \
	caja-src-marshal.c \
	caja-src-marshal.h \
	$(dbus_freedesktop_built_sources) \
	$(NULL)
caja_SOURCES = \
	caja-actions.h \
	caja-application.c \
	caja-application.h \
	caja-bookmark-list.c \
	caja-bookmark-list.h \
	caja-bookmarks-window.c \
	caja-bookmarks-window.h \
	caja-connect-server-dialog.c \
	caja-connect-server-dialog.h \
	caja-connect-server-dialog-nonmain.c \
	caja-connect-server-operation.c	\
	caja-connect-server-operation.h	\
	caja-desktop-window.c \
	caja-desktop-window.h \
	caja-emblem-sidebar.c \
	caja-emblem-sidebar.h \
	caja-file-management-properties.c \
	caja-file-management-properties.h \
	caja-freedesktop-dbus.c \
	caja-freedesktop-dbus.h \
	caja-history-sidebar.c \
	caja-history-sidebar.h \
	caja-image-properties-page.c \
	caja-image-properties-page.h \
	caja-information-panel.c \
	caja-information-panel.h \
	caja-location-bar.c \
	caja-location-bar.h \
	caja-location-dialog.c \
	caja-location-dialog.h \
	caja-location-entry.c \
	caja-location-entry.h \
	caja-main.c \
	caja-navigation-action.c \
	caja-navigation-action.h \
	caja-navigation-window-menus.c \
	caja-navigation-window.c \
	caja-navigation-window.h \
	caja-navigation-window-pane.c \
	caja-navigation-window-pane.h \
	caja-navigation-window-slot.c \
	caja-navigation-window-slot.h \
	caja-notebook.c \
	caja-notebook.h \
	caja-notes-viewer.c \
	caja-notes-viewer.h \
	caja-pathbar.c \
	caja-pathbar.h \
	caja-places-sidebar.c \
	caja-places-sidebar.h \
	caja-property-browser.c \
	caja-property-browser.h \
	caja-query-editor.c \
	caja-query-editor.h \
	caja-search-bar.c \
	caja-search-bar.h \
	caja-self-check-functions.c \
	caja-self-check-functions.h \
	caja-side-pane.c \
	caja-side-pane.h \
	caja-sidebar-title.c \
	caja-sidebar-title.h \
	caja-spatial-window.c \
	caja-spatial-window.h \
	caja-trash-bar.c \
	caja-trash-bar.h \
	caja-view-as-action.c \
	caja-view-as-action.h \
	caja-window-bookmarks.c \
	caja-window-bookmarks.h \
	caja-window-manage-views.c \
	caja-window-manage-views.h \
	caja-window-menus.c \
	caja-window-pane.c \
	caja-window-pane.h \
	caja-window-private.h \
	caja-window-slot.c \
	caja-window-slot.h \
	caja-window-toolbars.c \
	caja-window.c \
	caja-window.h \
	caja-x-content-bar.c \
	caja-x-content-bar.h \
	caja-zoom-action.c \
	caja-zoom-action.h \
	caja-zoom-control.c \
	caja-zoom-control.h \
	$(NULL)

nodist_caja_SOURCES = \
	$(BUILT_SOURCES) \
	$(NULL)

caja-src-marshal.list: $(caja_SOURCES) Makefile.am
	$(AM_V_GEN)( cd $(srcdir) && \
	sed -n -e 's/.*caja_src_marshal_\([[:upper:][:digit:]]*__[[:upper:][:digit:]_]*\).*/\1/p' \
	$(caja_SOURCES) ) \
	| sed -e 's/__/:/' -e 'y/_/,/' | sort -u > $@.tmp
	@if cmp -s $@.tmp $@; then \
		rm $@.tmp; \
	else \
		mv $@.tmp $@; \
	fi

%-marshal.c: %-marshal.list Makefile
	$(AM_V_GEN)echo "#include \"caja-src-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) --body --prefix=$(subst -,_,$*)_marshal $< >> $*-marshal.c

%-marshal.h: %-marshal.list Makefile
	$(AM_V_GEN)$(GLIB_GENMARSHAL) --header --prefix=$(subst -,_,$*)_marshal $< > $*-marshal.h

caja_file_management_properties_SOURCES = \
	caja-file-management-properties.c \
	caja-file-management-properties.h \
	caja-file-management-properties-main.c \
	$(NULL)

caja_autorun_software_SOURCES = \
	caja-autorun-software.c \
	$(NULL)

caja_connect_server_SOURCES = \
	caja-bookmark-list.c \
	caja-bookmark-list.h \
	caja-connect-server-dialog.c \
	caja-connect-server-dialog.h \
	caja-connect-server-dialog-main.c \
	caja-connect-server-operation.c     \
	caja-connect-server-operation.h     \
	$(NULL)


aboutdir = $(datadir)/caja
about_DATA = \
	caja.about \
        $(NULL)

if ENABLE_SELF_CHECK
TESTS=check-caja
endif

uidir = $(datadir)/caja/ui

ui_files = \
	caja-file-management-properties.ui \
	caja-bookmarks-window.ui \
	$(NULL)

include $(top_srcdir)/gla11y.mk

ui_DATA = \
	caja-shell-ui.xml \
	caja-navigation-window-ui.xml \
	caja-spatial-window-ui.xml \
	$(ui_files) \
	$(NULL)

CLEANFILES = \
	caja-src-marshal.list \
	$(BUILT_SOURCES) \
	$(server_DATA) \
	$(NULL)

EXTRA_DIST = \
	caja-src-marshal.list \
	$(server_in_files) \
	$(ui_DATA) \
	check-caja \
	$(about_DATA) \
	ui-a11y.suppr \
	$(NULL)

dist-hook:
	cd $(distdir); rm -f $(CLEANFILES)

-include $(top_srcdir)/git.mk
