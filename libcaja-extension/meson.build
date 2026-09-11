libcaja_extension_sources = files(
  'caja-column-provider.c',
  'caja-column.c',
  'caja-configurable.c',
  'caja-extension-types.c',
  'caja-extension-private.h',
  'caja-file-info.c',
  'caja-info-provider.c',
  'caja-location-widget-provider.c',
  'caja-menu-item.c',
  'caja-menu-provider.c',
  'caja-menu.c',
  'caja-property-page-provider.c',
  'caja-property-page.c',
  'caja-widget-view-provider.c',
)

libcaja_extension_headers = files(
  'caja-column-provider.h',
  'caja-column.h',
  'caja-configurable.h',
  'caja-extension-types.h',
  'caja-file-info.h',
  'caja-info-provider.h',
  'caja-location-widget-provider.h',
  'caja-menu-item.h',
  'caja-menu-provider.h',
  'caja-menu.h',
  'caja-property-page-provider.h',
  'caja-property-page.h',
  'caja-widget-view-provider.h',
)

install_headers(
  libcaja_extension_headers,
  subdir: 'caja/libcaja-extension',
)

libcaja_extension = library(
  'caja-extension',
  sources: libcaja_extension_sources,
  version: caja_extension_libversion,
  soversion: caja_extension_soversion,
  dependencies: libcaja_extension_deps,
  include_directories: include_directories('..'),
  install: true,
  install_dir: libdir,
)

libcaja_extension_dep = declare_dependency(
  link_with: libcaja_extension,
  include_directories: include_directories('.'),
  dependencies: libcaja_extension_deps,
)

pkg.generate(
  libcaja_extension,
  version: package_version,
  name: 'libcaja-extension',
  description: 'A library to create Caja view extensions',
  filebase: 'libcaja-extension',
  subdirs: 'caja',
  requires: ['glib-2.0', 'gio-2.0', 'gtk+-3.0'],
  install_dir: pkgconfigdir,
  variables: [
    'extensiondir=' + extensiondir,
  ],
)

if enable_introspection
  gir_sources = []
  foreach s : libcaja_extension_sources
    gir_sources += s
  endforeach
  foreach h : libcaja_extension_headers
    gir_sources += h
  endforeach

  gnome.generate_gir(
    libcaja_extension,
    sources: gir_sources,
    nsversion: '2.0',
    namespace: 'Caja',
    symbol_prefix: 'caja_',
    identifier_prefix: 'Caja',
    export_packages: 'libcaja-extension',
    includes: ['GObject-2.0', 'Gtk-3.0', 'Gio-2.0'],
    extra_args: [
      '--warn-all',
      '--add-include-path=' + meson.current_source_dir(),
    ],
    install: true,
    install_dir_gir: join_paths(datadir, 'gir-1.0'),
    install_dir_typelib: join_paths(libdir, 'girepository-1.0'),
  )
endif
