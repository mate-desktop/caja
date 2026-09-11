gtkdoc_src_dir = join_paths(meson.project_source_root(), 'libcaja-extension')

version_xml = configure_file(
  input: 'version.xml.in',
  output: 'version.xml',
  configuration: {
    'CAJA_EXTENSION_VERSION_INFO': '@0@:@1@:@2@'.format(
      caja_extension_current,
      caja_extension_revision,
      caja_extension_age,
    ),
  },
)

gnome.gtkdoc(
  'libcaja-extension',
  src_dir: gtkdoc_src_dir,
  main_sgml: 'libcaja-extension-docs.xml',
  scan_args: '--ignore-headers=' + ' '.join([
    'config.h',
  ]),
  mkdb_args: ['--output-format=xml'],
  content_files: [version_xml],
  dependencies: libcaja_extension_dep,
  install: true,
)