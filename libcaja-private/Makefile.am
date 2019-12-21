include $(top_srcdir)/Makefile.shared

noinst_LTLIBRARIES=libcaja-private.la

AM_CPPFLAGS = \
	-I$(top_srcdir) \
	-I$(top_builddir) \
	$(CORE_CFLAGS) \
	$(WARNING_CFLAGS) \
	$(DISABLE_DEPRECATED_CFLAGS) \
	$(TRACKER_CFLAGS) \
	$(BEAGLE_CFLAGS) \
	-DDATADIR=\""$(datadir)"\" \
	-DSYSCONFDIR=\""$(sysconfdir)"\" \
	-DCAJA_DATADIR=\""$(datadir)/caja"\" \
	-DCAJA_EXTENSIONDIR=\""$(libdir)/caja/extensions-2.0"\" \
	$(NULL)

BUILT_SOURCES = \
	caja-marshal.c \
	caja-marshal.h \
	$(NULL)

dependency_static_libs = \
	$(top_builddir)/libegg/libegg.la \
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
    -lnotify
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
	caja-extensions.c \
	caja-extensions.h \
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
	caja-keep-last-vertical-box.c \
	caja-keep-last-vertical-box.h \
	caja-lib-self-check-functions.c \
	caja-lib-self-check-functions.h \
	caja-link.c \
	caja-link.h \
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

nodist_libcaja_private_la_SOURCES =\
	$(BUILT_SOURCES) \
	$(NULL)

$(lib_LTLIBRARIES): $(dependency_static_libs)

caja-marshal.list: $(libcaja_private_la_SOURCES) Makefile.am
	$(AM_V_GEN)( cd $(srcdir) && \
	sed -n -e 's/.*caja_marshal_\([[:upper:][:digit:]]*__[[:upper:][:digit:]_]*\).*/\1/p' \
	$(libcaja_private_la_SOURCES) ) \
	| sed -e 's/__/:/' -e 'y/_/,/' | sort -u > $@.tmp
	@if cmp -s $@.tmp $@; then \
		rm $@.tmp; \
	else \
		mv $@.tmp $@; \
	fi

%-marshal.h: %-marshal.list Makefile
	$(AM_V_GEN)$(GLIB_GENMARSHAL) --header --prefix=$(subst -,_,$*)_marshal $< > $*-marshal.h

%-marshal.c: %-marshal.list Makefile
	$(AM_V_GEN)echo "#include \"caja-marshal.h\"" > $@ && \
	$(GLIB_GENMARSHAL) --body --prefix=$(subst -,_,$*)_marshal $< >> $*-marshal.c

gsettings_SCHEMAS = org.mate.caja.gschema.xml org.mate.media-handling.gschema.xml

@GSETTINGS_RULES@

EXTRA_DIST = \
	caja-marshal.list \
	$(gsettings_SCHEMAS) \
	$(BEAGLE_SOURCES) \
	$(TRACKER_SOURCES) \
	$(NULL)

CLEANFILES = \
	$(BUILT_SOURCES) \
	caja-marshal.list \
	$(NULL)

dist-hook:
	cd $(distdir); rm -f $(CLEANFILES)


-include $(top_srcdir)/git.mk
