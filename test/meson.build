test_cargs = [
  '-DVERSION="' + package_version + '"',
  '-DCAJA_DATADIR="' + join_paths(datadir, package_name) + '"',
  '-DMATELOCALEDIR="' + localedir + '"',
  '-DPIXBUFDIR="' + join_paths(datadir, 'pixmaps') + '"',
]

test_deps = [libcaja_private_dep] + core_deps

test_sources = files('test.c', 'test.h')

executable(
  'test-caja-wrap-table',
  sources: ['test-caja-wrap-table.c', test_sources],
  include_directories: include_directories('..'),
  dependencies: test_deps,
  c_args: test_cargs,
  install: false,
)

executable(
  'test-caja-search-engine',
  sources: ['test-caja-search-engine.c'],
  include_directories: include_directories('..'),
  dependencies: test_deps,
  c_args: test_cargs,
  install: false,
)

executable(
  'test-caja-directory-async',
  sources: ['test-caja-directory-async.c'],
  include_directories: include_directories('..'),
  dependencies: test_deps,
  c_args: test_cargs,
  install: false,
)

executable(
  'test-caja-copy',
  sources: ['test-copy.c', test_sources],
  include_directories: include_directories('..'),
  dependencies: test_deps,
  c_args: test_cargs,
  install: false,
)

executable(
  'test-eel-background',
  sources: ['test-eel-background.c'],
  include_directories: include_directories('..'),
  dependencies: test_deps,
  c_args: test_cargs,
  install: false,
)

executable(
  'test-eel-editable-label',
  sources: ['test-eel-editable-label.c'],
  include_directories: include_directories('..'),
  dependencies: test_deps,
  c_args: test_cargs,
  install: false,
)

executable(
  'test-eel-image-table',
  sources: ['test-eel-image-table.c', test_sources],
  include_directories: include_directories('..'),
  dependencies: test_deps,
  c_args: test_cargs,
  install: false,
)

executable(
  'test-eel-labeled-image',
  sources: ['test-eel-labeled-image.c', test_sources, 'test.h'],
  include_directories: include_directories('..'),
  dependencies: test_deps,
  c_args: test_cargs,
  install: false,
)

executable(
  'test-eel-pixbuf-scale',
  sources: ['test-eel-pixbuf-scale.c', test_sources, 'test.h'],
  include_directories: include_directories('..'),
  dependencies: test_deps,
  c_args: test_cargs,
  install: false,
)