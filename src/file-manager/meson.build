fm_marshal = gnome.genmarshal(
  'fm-marshal',
  sources: 'fm-marshal.list',
  prefix: 'fm_marshal',
)

fm_sources = [
  'fm-actions.h',
  'fm-desktop-icon-view.c',
  'fm-desktop-icon-view.h',
  'fm-desktop-wayland-bg-dialog.c',
  'fm-desktop-wayland-bg-dialog.h',
  'fm-directory-view.c',
  'fm-directory-view.h',
  'fm-widget-view.c',
  'fm-widget-view.h',
  'fm-ditem-page.c',
  'fm-ditem-page.h',
  'fm-error-reporting.c',
  'fm-error-reporting.h',
  'fm-icon-container.c',
  'fm-icon-container.h',
  'fm-icon-view.c',
  'fm-icon-view.h',
  'fm-list-model.c',
  'fm-list-model.h',
  'fm-list-view-private.h',
  'fm-list-view.c',
  'fm-list-view.h',
  'fm-properties-window.c',
  'fm-properties-window.h',
  'fm-tree-model.c',
  'fm-tree-model.h',
  'fm-tree-view.c',
  'fm-tree-view.h',
  'caja-audio-mime-types.h',
]

if enable_empty_view
  fm_sources += [
    'fm-empty-view.c',
    'fm-empty-view.h',
  ]
endif

fm_cargs = [
  '-DICONDIR="' + join_paths(datadir, 'icons') + '"',
  '-DPIXMAPDIR="' + join_paths(datadir, 'pixmaps') + '"',
  '-DCAJA_EXTENSIONDIR="' + extensiondir + '"',
]

libcaja_file_manager = static_library(
  'caja-file-manager',
  sources: [
    fm_sources,
    fm_marshal,
  ],
  include_directories: include_directories('..', '../../mate-submodules'),
  dependencies: libcaja_private_dep,
  c_args: fm_cargs,
  install: false,
)

libcaja_file_manager_dep = declare_dependency(
  link_with: libcaja_file_manager,
  include_directories: include_directories('.'),
  dependencies: libcaja_private_dep,
)

# Install UI definitions
install_data(
  'caja-desktop-icon-view-ui.xml',
  'caja-directory-view-ui.xml',
  'caja-icon-view-ui.xml',
  'caja-list-view-ui.xml',
  install_dir: join_paths(datadir, package_name, 'ui'),
)