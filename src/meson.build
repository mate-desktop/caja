subdir('file-manager')

cargs = [
  '-DG_LOG_DOMAIN="caja"',
  '-DPREFIX="' + prefix + '"',
  '-DVERSION="' + package_version + '"',
  '-DDATADIR="' + datadir + '"',
  '-DLIBDIR="' + libdir + '"',
  '-DCAJA_DATADIR="' + join_paths(datadir, package_name) + '"',
  '-DMATELOCALEDIR="' + localedir + '"',
  '-DCAJA_PIXMAPDIR="' + join_paths(datadir, 'pixmaps', package_name) + '"',
  '-DSYSCONFDIR="' + join_paths(prefix, get_option('sysconfdir')) + '"',
]

# GResource
caja_resources = gnome.compile_resources(
  'caja-resources',
  '../data/caja-resources.gresource.xml',
  c_name: 'caja',
  source_dir: '../data',
  install: false,
)

# D-Bus codegen
caja_freedesktop_generated = gnome.gdbus_codegen(
  'caja-freedesktop-generated',
  '../data/freedesktop-dbus-interfaces.xml',
  interface_prefix: 'org.freedesktop.',
  namespace: 'CajaFreedesktop',
  object_manager: true,
)

# Marshal
caja_src_marshal = gnome.genmarshal(
  'caja-src-marshal',
  sources: 'caja-src-marshal.list',
  prefix: 'caja_src_marshal',
)

caja_deps = core_deps + [
  libcaja_file_manager_dep,
  libcaja_private_dep,
  libcaja_extension_dep,
  libnotify_dep,
  libm_dep,
]

if exif_dep.found()
  caja_deps += [exif_dep]
endif
if exempi_dep.found()
  caja_deps += [exempi_dep]
endif
if enable_wayland
  caja_deps += wayland_deps
endif

common_sources = [
  'caja-actions.h',
  'caja-application.c',
  'caja-application.h',
  'caja-bookmark-list.c',
  'caja-bookmark-list.h',
  'caja-bookmarks-sidebar.c',
  'caja-bookmarks-sidebar.h',
  'caja-bookmarks-window.c',
  'caja-bookmarks-window.h',
  'caja-connect-server-dialog.c',
  'caja-connect-server-dialog.h',
  'caja-connect-server-dialog-nonmain.c',
  'caja-connect-server-operation.c',
  'caja-connect-server-operation.h',
  'caja-desktop-window.c',
  'caja-desktop-window.h',
  'caja-emblem-sidebar.c',
  'caja-emblem-sidebar.h',
  'caja-file-management-properties.c',
  'caja-file-management-properties.h',
  'caja-freedesktop-dbus.c',
  'caja-freedesktop-dbus.h',
  'caja-history-sidebar.c',
  'caja-history-sidebar.h',
  'caja-image-properties-page.c',
  'caja-image-properties-page.h',
  'caja-information-panel.c',
  'caja-information-panel.h',
  'caja-location-bar.c',
  'caja-location-bar.h',
  'caja-location-dialog.c',
  'caja-location-dialog.h',
  'caja-location-entry.c',
  'caja-location-entry.h',
  'caja-main.c',
  'caja-navigation-action.c',
  'caja-navigation-action.h',
  'caja-navigation-window-menus.c',
  'caja-navigation-window.c',
  'caja-navigation-window.h',
  'caja-navigation-window-pane.c',
  'caja-navigation-window-pane.h',
  'caja-navigation-window-slot.c',
  'caja-navigation-window-slot.h',
  'caja-notebook.c',
  'caja-notebook.h',
  'caja-notes-viewer.c',
  'caja-notes-viewer.h',
  'caja-pathbar.c',
  'caja-pathbar.h',
  'caja-places-sidebar.c',
  'caja-places-sidebar.h',
  'caja-property-browser.c',
  'caja-property-browser.h',
  'caja-query-editor.c',
  'caja-query-editor.h',
  'caja-search-bar.c',
  'caja-search-bar.h',
  'caja-self-check-functions.c',
  'caja-self-check-functions.h',
  'caja-side-pane.c',
  'caja-side-pane.h',
  'caja-sidebar-title.c',
  'caja-sidebar-title.h',
  'caja-spatial-window.c',
  'caja-spatial-window.h',
  'caja-trash-bar.c',
  'caja-trash-bar.h',
  'caja-view-as-action.c',
  'caja-view-as-action.h',
  'caja-window-bookmarks.c',
  'caja-window-bookmarks.h',
  'caja-window-manage-views.c',
  'caja-window-manage-views.h',
  'caja-window-menus.c',
  'caja-window-pane.c',
  'caja-window-pane.h',
  'caja-window-private.h',
  'caja-window-slot.c',
  'caja-window-slot.h',
  'caja-window-toolbars.c',
  'caja-window.c',
  'caja-window.h',
  'caja-x-content-bar.c',
  'caja-x-content-bar.h',
  'caja-zoom-action.c',
  'caja-zoom-action.h',
  'caja-zoom-control.c',
  'caja-zoom-control.h',
  caja_resources,
  caja_src_marshal,
  caja_freedesktop_generated,
]

caja = executable(
  'caja',
  common_sources,
  include_directories: include_directories('..', 'file-manager', '../mate-submodules'),
  dependencies: caja_deps,
  c_args: cargs,
  install: true,
)

executable(
  'caja-file-management-properties',
  sources: [
    'caja-file-management-properties.c',
    'caja-file-management-properties.h',
    'caja-file-management-properties-main.c',
    caja_resources,
  ],
  include_directories: include_directories('..', 'file-manager', '../mate-submodules'),
  dependencies: caja_deps,
  c_args: cargs,
  install: true,
)

executable(
  'caja-autorun-software',
  sources: [
    'caja-autorun-software.c',
  ],
  include_directories: include_directories('..', 'file-manager', '../mate-submodules'),
  dependencies: caja_deps,
  c_args: cargs,
  install: true,
)

executable(
  'caja-connect-server',
  sources: [
    'caja-bookmark-list.c',
    'caja-bookmark-list.h',
    'caja-connect-server-dialog.c',
    'caja-connect-server-dialog.h',
    'caja-connect-server-dialog-main.c',
    'caja-connect-server-operation.c',
    'caja-connect-server-operation.h',
  ],
  include_directories: include_directories('..', 'file-manager', '../mate-submodules'),
  dependencies: caja_deps,
  c_args: cargs,
  install: true,
)

# Install UI definitions
install_data(
  'caja-shell-ui.xml',
  'caja-navigation-window-ui.xml',
  'caja-spatial-window-ui.xml',
  install_dir: join_paths(datadir, package_name, 'ui'),
)

# Install about file
install_data(
  'caja.about',
  install_dir: join_paths(datadir, package_name),
)