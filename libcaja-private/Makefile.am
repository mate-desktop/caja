include $(top_srcdir)/Makefile.shared

noinst_LTLIBRARIES=libcaja-private.la

INCLUDES = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	-I$(top_srcdir)/cut-n-paste-code \
	$(CORE_CFLAGS) \
	$(WARNING_CFLAGS) \
	$(DISABLE_DEPRECATED_CFLAGS) \
	$(TRACKER_CFLAGS) \
	$(BEAGLE_CFLAGS) \
	-DDATADIR=\""$(datadir)"\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DCAJA_DATADIR=\""$(datadir)/caja"\" \
	-DCAJA_EXTENSIONDIR=\""$(libdir)/caja/extensions-2.0"\" \
	$(UNIQUE_CFLAGS) \
	$(NULL)

dependency_static_libs = \
	$(top_builddir)/cut-n-paste-code/libegg/libegg.la \
	$(NULL)

libcaja_private_la_LDFLAGS = \
	-no-undefined \
	$(NULL)

libcaja_private_la_LIBADD = \
	$(dependency_static_libs) \
	$(SELINUX_LIBS) \
	$(BEAGLE_LIBS) \
	$(TRACKER_LIBS) \
	$(top_builddir)/eel/libeel-2.la \
	$(top_builddir)/libcaja-extension/libcaja-extension.la \
	$(CORE_LIBS) \
	$(NULL)

marshal_sources = \
	caja-marshal.h \
	caja-marshal-guts.c \
	$(NULL)

libcaja_private_la_SOURCES = \
	caja-autorun.c \
	caja-autorun.h \
	caja-bookmark.c \
	caja-bookmark.h \
	caja-cell-renderer-text-ellipsized.c \
	caja-cell-renderer-text-ellipsized.h \
	caja-clipboard-monitor.c \
	caja-clipboard-monitor.h \
	caja-clipboard.c \
	caja-clipboard.h \
	caja-column-chooser.c \
	caja-column-chooser.h \
	caja-column-utilities.c \
	caja-column-utilities.h \
	caja-customization-data.c \
	caja-customization-data.h \
	caja-debug-log.c \
	caja-debug-log.h \
	caja-default-file-icon.c \
	caja-default-file-icon.h \
	caja-desktop-directory-file.c \
	caja-desktop-directory-file.h \
	caja-desktop-directory.c \
	caja-desktop-directory.h \
	caja-desktop-icon-file.c \
	caja-desktop-icon-file.h \
	caja-desktop-link-monitor.c \
	caja-desktop-link-monitor.h \
	caja-desktop-link.c \
	caja-desktop-link.h \
	caja-desktop-metadata.c \
	caja-desktop-metadata.h \
	caja-directory-async.c \
	caja-directory-background.c \
	caja-directory-background.h \
	caja-directory-notify.h \
	caja-directory-private.h \
	caja-directory.c \
	caja-directory.h \
	caja-dnd.c \
	caja-dnd.h \
	caja-emblem-utils.c \
	caja-emblem-utils.h \
	caja-entry.c \
	caja-entry.h \
	caja-file-attributes.h \
	caja-file-changes-queue.c \
	caja-file-changes-queue.h \
	caja-file-conflict-dialog.c \
	caja-file-conflict-dialog.h \
	caja-file-dnd.c \
	caja-file-dnd.h \
	caja-file-operations.c \
	caja-file-operations.h \
	caja-file-private.h \
	caja-file-queue.c \
	caja-file-queue.h \
	caja-file-utilities.c \
	caja-file-utilities.h \
	caja-file.c \
	caja-file.h \
	caja-global-preferences.c \
	caja-global-preferences.h \
	caja-horizontal-splitter.c \
	caja-horizontal-splitter.h \
	caja-icon-canvas-item.c \
	caja-icon-canvas-item.h \
	caja-icon-container.c \
	caja-icon-container.h \
	caja-icon-dnd.c \
	caja-icon-dnd.h \
	caja-icon-private.h \
	caja-icon-info.c \
	caja-icon-info.h \
	caja-icon-names.h \
	caja-idle-queue.c \
	caja-idle-queue.h \
	caja-keep-last-vertical-box.c \
	caja-keep-last-vertical-box.h \
	caja-lib-self-check-functions.c \
	caja-lib-self-check-functions.h \
	caja-link.c \
	caja-link.h \
	caja-marshal.c \
	caja-marshal.h \
	caja-merged-directory.c \
	caja-merged-directory.h \
	caja-metadata.h \
	caja-metadata.c \
	caja-mime-actions.c \
	caja-mime-actions.h \
	caja-mime-application-chooser.c \
	caja-mime-application-chooser.h \
	caja-module.c \
	caja-module.h \
	caja-monitor.c \
	caja-monitor.h \
	caja-open-with-dialog.c \
	caja-open-with-dialog.h \
	caja-progress-info.c \
	caja-progress-info.h \
	caja-program-choosing.c \
	caja-program-choosing.h \
	caja-recent.c \
	caja-recent.h \
	caja-saved-search-file.c \
	caja-saved-search-file.h \
	caja-search-directory.c \
	caja-search-directory.h \
	caja-search-directory-file.c \
	caja-search-directory-file.h \
	caja-search-engine.c \
	caja-search-engine.h \
	caja-search-engine-simple.c \
	caja-search-engine-simple.h \
	caja-search-engine-beagle.c \
	caja-search-engine-beagle.h \
	caja-search-engine-tracker.c \
	caja-search-engine-tracker.h \
	caja-sidebar-provider.c \
	caja-sidebar-provider.h \
	caja-sidebar.c \
	caja-sidebar.h \
	caja-signaller.h \
	caja-signaller.c \
	caja-query.c \
	caja-query.h \
	caja-thumbnails.c \
	caja-thumbnails.h \
	caja-trash-monitor.c \
	caja-trash-monitor.h \
	caja-tree-view-drag-dest.c \
	caja-tree-view-drag-dest.h \
	caja-ui-utilities.c \
	caja-ui-utilities.h \
	caja-users-groups-cache.c \
	caja-users-groups-cache.h \
	caja-vfs-directory.c \
	caja-vfs-directory.h \
	caja-vfs-file.c \
	caja-vfs-file.h \
	caja-view-factory.c \
	caja-view-factory.h \
	caja-view.c \
	caja-view.h \
	caja-window-info.c \
	caja-window-info.h \
	caja-window-slot-info.c \
	caja-window-slot-info.h \
	caja-undostack-manager.c \
	caja-undostack-manager.h \
	$(NULL)

$(lib_LTLIBRARIES): $(dependency_static_libs)

caja-marshal.h: caja-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN)$(GLIB_GENMARSHAL) $< --header --prefix=caja_marshal > $@
caja-marshal-guts.c: caja-marshal.list $(GLIB_GENMARSHAL)
	$(AM_V_GEN)$(GLIB_GENMARSHAL) $< --body --prefix=caja_marshal > $@

$(libcaja_private_la_OBJECTS): $(marshal_sources)

gsettingsschema_in_files = org.mate.caja.gschema.xml.in org.mate.media-handling.gschema.xml.in
gsettings_SCHEMAS = $(gsettingsschema_in_files:.xml.in=.xml)
.PRECIOUS: $(gsettings_SCHEMAS)

@INTLTOOL_XML_NOMERGE_RULE@

@GSETTINGS_RULES@

EXTRA_DIST = \
	caja-marshal.list \
	$(gsettingsschema_in_files) \
	$(BEAGLE_SOURCES) \
	$(TRACKER_SOURCES) \
	$(NULL)

CLEANFILES = \
	$(marshal_sources) \
	$(schema_DATA) \
	$(gsettings_SCHEMAS) \
	$(NULL)

dist-hook:
	cd $(distdir); rm -f $(CLEANFILES)

