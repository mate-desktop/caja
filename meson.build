project('caja', 'c', version: '1.19.2')

cc = meson.get_compiler('c')
m_dep = cc.find_library('m', required : false)
x11_dep = cc.find_library('X11')

exempi_dep       = dependency('exempi-2.0',       version : '>=1.99.5')
gail_dep         = dependency('gail-3.0',         version : '>=3.0.0')
gio_dep          = dependency('gio-2.0',          version : '>=2.26.0')
gio_unix_dep     = dependency('gio-unix-2.0')
glib_dep         = dependency('glib-2.0',         version : '>=2.36.0')
gmodule_dep      = dependency('gmodule-2.0')
gthread_dep      = dependency('gthread-2.0')
gtk_dep          = dependency('gtk+-3.0',         version : '>=3.14.0')
libexif_dep      = dependency('libexif',          version : '>=0.6.14')
libnotify_dep    = dependency('libnotify',        version : '>=0.7.0')
libselinux_dep   = dependency('libselinux')
libxml_dep       = dependency('libxml-2.0',       version : '>=2.4.7')
mate_desktop_dep = dependency('mate-desktop-2.0', version : '>=1.17.3')
pango_dep        = dependency('pango',            version : '>=1.1.2')

add_global_arguments([
	'-DCAJA_DATADIR="' + get_option('datadir') + '"',
	'-DCAJA_EXTENSIONDIR="' + join_paths(get_option('libdir'), 'caja/extensions-2.0') + '"',
	'-DDATADIR="' + get_option('datadir') + '"',
], language : 'c')

libegg = static_library('egg', [
	'cut-n-paste-code/libegg/eggdesktopfile.c',
	'cut-n-paste-code/libegg/eggsmclient.c',
	'cut-n-paste-code/libegg/eggsmclient-xsmp.c',
	'cut-n-paste-code/libegg/eggtreemultidnd.c',
], dependencies : [gtk_dep])

libeel = static_library('eel', [
	'eel/eel-accessibility.c',
	'eel/eel-art-extensions.c',
	'eel/eel-art-gtk-extensions.c',
	'eel/eel-background-box.c',
	'eel/eel-background.c',
	'eel/eel-canvas-rect-ellipse.c',
	'eel/eel-canvas.c',
	'eel/eel-debug.c',
	'eel/eel-editable-label.c',
	'eel/eel-gdk-extensions.c',
	'eel/eel-gdk-pixbuf-extensions.c',
	'eel/eel-glib-extensions.c',
	'eel/eel-graphic-effects.c',
	'eel/eel-gtk-container.c',
	'eel/eel-gtk-extensions.c',
	'eel/eel-image-table.c',
	'eel/eel-labeled-image.c',
	'eel/eel-lib-self-check-functions.c',
	'eel/eel-mate-extensions.c',
	'eel/eel-self-checks.c',
	'eel/eel-stock-dialogs.c',
	'eel/eel-string.c',
	'eel/eel-vfs-extensions.c',
	'eel/eel-wrap-table.c',
	'eel/eel-xml-extensions.c',
], dependencies : [gtk_dep, gail_dep, libxml_dep, mate_desktop_dep])

libcaja_private = static_library('caja-private', [
	'libcaja-private/caja-autorun.c',
	'libcaja-private/caja-bookmark.c',
	'libcaja-private/caja-cell-renderer-text-ellipsized.c',
	'libcaja-private/caja-clipboard-monitor.c',
	'libcaja-private/caja-clipboard.c',
	'libcaja-private/caja-column-chooser.c',
	'libcaja-private/caja-column-utilities.c',
	'libcaja-private/caja-customization-data.c',
	'libcaja-private/caja-debug-log.c',
	'libcaja-private/caja-default-file-icon.c',
	'libcaja-private/caja-desktop-directory-file.c',
	'libcaja-private/caja-desktop-directory.c',
	'libcaja-private/caja-desktop-icon-file.c',
	'libcaja-private/caja-desktop-link-monitor.c',
	'libcaja-private/caja-desktop-link.c',
	'libcaja-private/caja-desktop-metadata.c',
	'libcaja-private/caja-directory-async.c',
	'libcaja-private/caja-directory-background.c',
	'libcaja-private/caja-directory.c',
	'libcaja-private/caja-dnd.c',
	'libcaja-private/caja-emblem-utils.c',
	'libcaja-private/caja-entry.c',
	'libcaja-private/caja-extensions.c',
	'libcaja-private/caja-file-changes-queue.c',
	'libcaja-private/caja-file-conflict-dialog.c',
	'libcaja-private/caja-file-dnd.c',
	'libcaja-private/caja-file-operations.c',
	'libcaja-private/caja-file-queue.c',
	'libcaja-private/caja-file-utilities.c',
	'libcaja-private/caja-file.c',
	'libcaja-private/caja-global-preferences.c',
	'libcaja-private/caja-icon-canvas-item.c',
	'libcaja-private/caja-icon-container.c',
	'libcaja-private/caja-icon-dnd.c',
	'libcaja-private/caja-icon-info.c',
	'libcaja-private/caja-keep-last-vertical-box.c',
	'libcaja-private/caja-lib-self-check-functions.c',
	'libcaja-private/caja-link.c',
	'libcaja-private/caja-metadata.c',
	'libcaja-private/caja-mime-actions.c',
	'libcaja-private/caja-mime-application-chooser.c',
	'libcaja-private/caja-module.c',
	'libcaja-private/caja-monitor.c',
	'libcaja-private/caja-open-with-dialog.c',
	'libcaja-private/caja-program-choosing.c',
	'libcaja-private/caja-progress-info.c',
	'libcaja-private/caja-query.c',
	'libcaja-private/caja-recent.c',
	'libcaja-private/caja-saved-search-file.c',
	'libcaja-private/caja-search-directory-file.c',
	'libcaja-private/caja-search-directory.c',
	'libcaja-private/caja-search-engine-beagle.c',
	'libcaja-private/caja-search-engine-simple.c',
	'libcaja-private/caja-search-engine-tracker.c',
	'libcaja-private/caja-search-engine.c',
	'libcaja-private/caja-sidebar-provider.c',
	'libcaja-private/caja-sidebar.c',
	'libcaja-private/caja-signaller.c',
	'libcaja-private/caja-thumbnails.c',
	'libcaja-private/caja-trash-monitor.c',
	'libcaja-private/caja-tree-view-drag-dest.c',
	'libcaja-private/caja-ui-utilities.c',
	'libcaja-private/caja-undostack-manager.c',
	'libcaja-private/caja-vfs-directory.c',
	'libcaja-private/caja-vfs-file.c',
	'libcaja-private/caja-view-factory.c',
	'libcaja-private/caja-view.c',
	'libcaja-private/caja-window-info.c',
	'libcaja-private/caja-window-slot-info.c',
], dependencies : [gtk_dep, libxml_dep, mate_desktop_dep, gail_dep])

libcaja_extension = static_library('caja-extension', [
	'libcaja-extension/caja-column-provider.c',
	'libcaja-extension/caja-column.c',
	'libcaja-extension/caja-extension-types.c',
	'libcaja-extension/caja-file-info.c',
	'libcaja-extension/caja-info-provider.c',
	'libcaja-extension/caja-location-widget-provider.c',
	'libcaja-extension/caja-menu-item.c',
	'libcaja-extension/caja-menu-provider.c',
	'libcaja-extension/caja-menu.c',
	'libcaja-extension/caja-property-page-provider.c',
	'libcaja-extension/caja-property-page.c',
], dependencies : [gtk_dep])


gnome = import('gnome')

caja_freedesktop_generated = gnome.gdbus_codegen(
	'caja-freedesktop-generated', 'data/freedesktop-dbus-interfaces.xml',
	interface_prefix : 'org.freedesktop.',
	namespace : 'CajaFreedesktop',
)

caja_src = [
	'src/caja-application.c',
	'src/caja-bookmark-list.c',
	'src/caja-bookmarks-window.c',
	'src/caja-connect-server-dialog-nonmain.c',
	'src/caja-connect-server-dialog.c',
	'src/caja-connect-server-operation.c',
	'src/caja-desktop-window.c',
	'src/caja-emblem-sidebar.c',
	'src/caja-file-management-properties.c',
	'src/caja-freedesktop-dbus.c',
	'src/caja-history-sidebar.c',
	'src/caja-image-properties-page.c',
	'src/caja-information-panel.c',
	'src/caja-location-bar.c',
	'src/caja-location-dialog.c',
	'src/caja-location-entry.c',
	'src/caja-main.c',
	'src/caja-navigation-action.c',
	'src/caja-navigation-window-menus.c',
	'src/caja-navigation-window-pane.c',
	'src/caja-navigation-window-slot.c',
	'src/caja-navigation-window.c',
	'src/caja-notebook.c',
	'src/caja-notes-viewer.c',
	'src/caja-pathbar.c',
	'src/caja-places-sidebar.c',
	'src/caja-property-browser.c',
	'src/caja-query-editor.c',
	'src/caja-search-bar.c',
	'src/caja-self-check-functions.c',
	'src/caja-side-pane.c',
	'src/caja-sidebar-title.c',
	'src/caja-spatial-window.c',
	'src/caja-trash-bar.c',
	'src/caja-view-as-action.c',
	'src/caja-window-bookmarks.c',
	'src/caja-window-manage-views.c',
	'src/caja-window-menus.c',
	'src/caja-window-pane.c',
	'src/caja-window-slot.c',
	'src/caja-window-toolbars.c',
	'src/caja-window.c',
	'src/caja-x-content-bar.c',
	'src/caja-zoom-action.c',
	'src/caja-zoom-control.c',
	'src/file-manager/fm-desktop-icon-view.c',
	'src/file-manager/fm-directory-view.c',
	'src/file-manager/fm-ditem-page.c',
	'src/file-manager/fm-error-reporting.c',
	'src/file-manager/fm-icon-container.c',
	'src/file-manager/fm-icon-view.c',
	'src/file-manager/fm-list-model.c',
	'src/file-manager/fm-list-view.c',
	'src/file-manager/fm-properties-window.c',
	'src/file-manager/fm-tree-model.c',
	'src/file-manager/fm-tree-view.c',
]

caja_marshal = gnome.genmarshal('caja-marshal',
	sources : 'libcaja-private/caja-marshal.list',
	prefix : 'caja_marshal',
)

caja_src_marshal_list = custom_target('caja-src-marshal-list',
	output : 'caja-src-marshal.list',
	input : caja_src,
	command : [
		find_program('./marshal.sh'), '@OUTPUT@', '@INPUT@'
	],
)

caja_src_marshal = gnome.genmarshal('caja-src-marshal',
	sources : caja_src_marshal_list,
	prefix : 'caja_src_marshal',
)

fm_marshal = gnome.genmarshal('fm-marshal',
	sources : 'src/file-manager/fm-marshal.list',
	prefix : 'fm_marshal',
)

eel_marshal = gnome.genmarshal('eel-marshal',
	sources : 'eel/eel-marshal.list',
	prefix : 'eel_marshal',
)

executable('caja', caja_src + [
	caja_freedesktop_generated,
	caja_marshal,
	caja_src_marshal,
	fm_marshal,
	eel_marshal,
], dependencies : [
	exempi_dep,
	gail_dep,
	gmodule_dep,
	gtk_dep,
	libexif_dep,
	libnotify_dep,
	libselinux_dep,
	libxml_dep,
	m_dep,
	mate_desktop_dep,
	x11_dep,
], c_args : [
	'-DUIDIR="' + join_paths(get_option('datadir'), 'share/caja') + '"',
	'-DCAJA_PIXMAPDIR="' + join_paths(get_option('datadir'), 'pixmaps/caja') + '"',
], link_with : [
	libegg,
	libeel,
	libcaja_private,
	libcaja_extension,
], include_directories : include_directories('cut-n-paste-code'))
