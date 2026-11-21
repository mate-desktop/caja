desktop_files = [
  'caja.desktop.in.in',
  'caja-home.desktop.in.in',
  'caja-computer.desktop.in.in',
  'caja-folder-handler.desktop.in.in',
  'caja-file-management-properties.desktop.in.in',
  'caja-browser.desktop.in.in',
  'caja-autorun-software.desktop.in.in',
  'mate-network-scheme.desktop.in.in',
]

foreach desktop_file : desktop_files
  desktop_in = desktop_file.replace('.in.in', '.in')
  desktop = desktop_file.replace('.in.in', '')

  desktop_in_file = configure_file(
    input: desktop_file,
    output: desktop_in,
    configuration: {
      'bindir': bindir,
    },
  )

  i18n.merge_file(
    input: desktop_in_file,
    output: desktop,
    type: 'desktop',
    po_dir: '../po',
    install: true,
    install_dir: join_paths(datadir, 'applications'),
  )
endforeach

# MIME type
mime_xml = i18n.merge_file(
  input: 'caja.xml.in',
  output: 'caja.xml',
  type: 'xml',
  po_dir: '../po',
  install: true,
  install_dir: join_paths(datadir, 'mime', 'packages'),
)

# D-Bus service
install_data(
  configure_file(
    input: 'org.mate.freedesktop.FileManager1.service.in',
    output: 'org.mate.freedesktop.FileManager1.service',
    configuration: {
      'bindir': bindir,
    },
  ),
  install_dir: join_paths(datadir, 'dbus-1', 'services'),
)

# AppData
appdata_in = configure_file(
  input: 'caja.appdata.xml.in.in',
  output: 'caja.appdata.xml.in',
  configuration: {
    'bindir': bindir,
  },
)

i18n.merge_file(
  input: appdata_in,
  output: 'caja.appdata.xml',
  type: 'xml',
  po_dir: '../po',
  install: true,
  install_dir: join_paths(datadir, 'metainfo'),
)

# Caja data files
install_data(
  'browser.xml',
  'caja-extras.placeholder',
  'caja-suggested.placeholder',
  'caja.css',
  'caja-desktop.css',
  'a11y-caja-desktop-base.css',
  'caja-desktop-ContrastHigh.css',
  'caja-desktop-ContrastHighInverse.css',
  'caja-desktop-HighContrast.css',
  'caja-desktop-HighContrastInverse.css',
  install_dir: join_paths(datadir, package_name),
)

# UI files used for resources
install_data(
  'caja-file-management-properties.ui',
  'caja-bookmarks-window.ui',
  install_dir: join_paths(datadir, package_name, 'ui'),
)

subdir('icons')
subdir('patterns')