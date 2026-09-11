project(
  'caja', 'c',
  version: '1.29.1',
  meson_version: '>=0.60.0',
  license: 'GPL-2.0+',
  default_options: [
    'warning_level=1',
    'buildtype=debugoptimized',
    'c_std=gnu11',
  ],
)

package_name = meson.project_name()
package_version = meson.project_version()

version_arr = package_version.split('.')
version_major = version_arr[0].to_int()
version_minor = version_arr[1].to_int()
version_micro = version_arr[2].to_int()

# caja-extension soversion from libtool version-info 5:0:4:
#   soname   = current - age        = 1
#   filename = {c-a}.{c}.{r}        = 1.5.0
caja_extension_current = 5
caja_extension_revision = 0
caja_extension_age = caja_extension_current - 1
caja_extension_soversion = caja_extension_current - caja_extension_age
caja_extension_libversion = '@0@.@1@.@2@'.format(
  caja_extension_soversion,
  caja_extension_current,
  caja_extension_revision,
)

cc = meson.get_compiler('c')

config_h = configuration_data()

config_h.set_quoted('PACKAGE', package_name)
config_h.set_quoted('PACKAGE_NAME', package_name)
config_h.set_quoted('PACKAGE_URL', 'https://mate-desktop.org')
config_h.set_quoted('PACKAGE_STRING', package_name + ' ' + package_version)
config_h.set_quoted('PACKAGE_VERSION', package_version)
config_h.set_quoted('GETTEXT_PACKAGE', package_name)
config_h.set_quoted('VERSION', package_version)

add_project_arguments(
  [
    '-DHAVE_CONFIG_H',
    '-I' + meson.project_source_root(),
    '-I' + meson.project_build_root(),
  ],
  language: 'c',
)

# Paths
prefix = get_option('prefix')
bindir = join_paths(prefix, get_option('bindir'))
datadir = join_paths(prefix, get_option('datadir'))
includedir = join_paths(prefix, get_option('includedir'))
libdir = join_paths(prefix, get_option('libdir'))
libexecdir = join_paths(prefix, get_option('libexecdir'))
localedir = join_paths(prefix, get_option('localedir'))
mandir = join_paths(prefix, get_option('mandir'))
pkgconfigdir = join_paths(libdir, 'pkgconfig')
extensiondir = join_paths(libdir, package_name, 'extensions-2.0')
schemadir = join_paths(datadir, 'glib-2.0', 'schemas')

# Meson modules
gnome = import('gnome')
i18n = import('i18n')
pkg = import('pkgconfig')

# Required dependency versions
gdkpixbuf_req = '>=2.36.5'
glib_req = '>=2.58.1'
gio_req = '>=2.50.0'
mate_desktop_req = '>=1.17.3'
pango_req = '>=1.1.2'
gtk_req = '>=3.22.0'
xml_req = '>=2.4.7'
exif_req = '>=0.6.14'
exempi_req = '>=1.99.5'
gail_req = '>=3.0.0'
notify_req = '>=0.7.0'
gtk_layer_shell_req = '>=0.8.0'

# Required dependencies
gdk_pixbuf_dep = dependency('gdk-pixbuf-2.0', version: gdkpixbuf_req)
glib_dep = dependency('glib-2.0', version: glib_req)
gthread_dep = dependency('gthread-2.0')
gio_unix_dep = dependency('gio-unix-2.0')
gio_dep = dependency('gio-2.0', version: gio_req)
pango_dep = dependency('pango', version: pango_req)
gtk_dep = dependency('gtk+-3.0', version: gtk_req)
libnotify_dep = dependency('libnotify', version: notify_req)
libxml_dep = dependency('libxml-2.0', version: xml_req)
gail_dep = dependency('gail-3.0', version: gail_req)
mate_desktop_dep = dependency('mate-desktop-2.0', version: mate_desktop_req)
gmodule_dep = dependency('gmodule-2.0', required: false)
x11_dep = dependency('x11')

core_deps = [
  glib_dep,
  gdk_pixbuf_dep,
  gio_dep,
  gio_unix_dep,
  gmodule_dep,
  gthread_dep,
  gtk_dep,
  gail_dep,
  mate_desktop_dep,
  pango_dep,
  libxml_dep,
  x11_dep,
]

core_cflags = []
core_link_args = []

libcaja_extension_deps = [
  glib_dep,
  gtk_dep,
]

# Optional dependencies
exif_dep = dependency('libexif', version: exif_req, required: false)
config_h.set('HAVE_EXIF', exif_dep.found())

exempi_dep = dependency('exempi-2.0', version: exempi_req,
  required: get_option('xmp'))
config_h.set('HAVE_EXEMPI', exempi_dep.found())

wayland_client_dep = dependency('wayland-client', required: false)
gtk_layer_shell_dep = dependency('gtk-layer-shell-0', version: gtk_layer_shell_req, required: false)
have_wayland = wayland_client_dep.found() and gtk_layer_shell_dep.found()
wayland_deps = [gtk_layer_shell_dep, wayland_client_dep]

enable_wayland = false
if get_option('wayland').disabled()
  enable_wayland = false
elif get_option('wayland').enabled()
  if not have_wayland
    error('Wayland support requested but gtk-layer-shell-0 not found')
  endif
  enable_wayland = true
else
  enable_wayland = have_wayland
endif
config_h.set('HAVE_WAYLAND', enable_wayland)

ice_dep = dependency('ice', required: false)
sm_dep = dependency('sm', required: false)
libegg_deps = [gtk_dep]
if ice_dep.found() and sm_dep.found()
  libegg_deps += [ice_dep, sm_dep]
endif

selinux_dep = cc.find_library('selinux', required: false)
have_selinux = selinux_dep.found() and cc.has_function('is_selinux_enabled', dependencies: selinux_dep)
config_h.set('HAVE_SELINUX', have_selinux)

libm_dep = cc.find_library('m', required: false)

# Self-check option
enable_self_check = get_option('self-check')
if not enable_self_check
  config_h.set('EEL_OMIT_SELF_CHECK', 1)
  config_h.set('CAJA_OMIT_SELF_CHECK', 1)
endif

# PackageKit option
enable_packagekit = get_option('packagekit')
config_h.set('ENABLE_PACKAGEKIT', enable_packagekit)

# Empty view option
enable_empty_view = get_option('empty-view')
if enable_empty_view
  config_h.set('ENABLE_EMPTY_VIEW', 1)
endif

# Header checks (mirror AC_CHECK_HEADERS)
foreach header : ['sys/mount.h', 'sys/vfs.h', 'sys/param.h', 'malloc.h']
  if cc.has_header(header)
    config_h.set('HAVE_@0@'.format(header.underscorify().to_upper()), 1)
  endif
endforeach

if cc.has_header('X11/XF86keysym.h')
  config_h.set('HAVE_X11_XF86KEYSYM_H', 1)
endif

# Function checks (mirror AC_CHECK_FUNCS)
if cc.has_function('mallopt', prefix: '#include <malloc.h>')
  config_h.set('HAVE_MALLOPT', 1)
endif

# Common defines for data dirs
config_h.set_quoted('DATADIR', datadir)
config_h.set_quoted('LIBDIR', libdir)
config_h.set_quoted('CAJA_DATADIR', join_paths(datadir, package_name))
config_h.set_quoted('MATELOCALEDIR', localedir)
config_h.set_quoted('CAJA_PIXMAPDIR', join_paths(datadir, 'pixmaps', package_name))
config_h.set_quoted('PREFIX', prefix)
config_h.set_quoted('SYSCONFDIR', join_paths(prefix, get_option('sysconfdir')))
config_h.set_quoted('CAJA_EXTENSIONDIR', extensiondir)

configure_file(
  output: 'config.h',
  configuration: config_h,
)

enable_gtk_doc = get_option('gtk-doc')
enable_introspection = get_option('introspection')

# Build order:
# eel, libcaja-extension -> mate-submodules/libegg -> libcaja-private -> src/file-manager -> src -> test -> data -> icons -> po -> docs
subdir('eel')
subdir('libcaja-extension')
subdir('mate-submodules')
subdir('libcaja-private')
subdir('src')
subdir('test')
subdir('data')
subdir('icons')
subdir('po')

if enable_gtk_doc
  subdir('docs')
endif

meson.add_install_script(
  'meson_post_install.sh',
  get_option('update-mimedb') ? 'yes' : 'no',
  get_option('icon-update') ? 'yes' : 'no',
)

summary({
  'prefix': prefix,
  'source code location': meson.current_source_dir(),
  'compiler': cc.get_id(),
  'XMP support': exempi_dep.found(),
  'PackageKit support': enable_packagekit,
  'Self check': enable_self_check,
  'Wayland support': enable_wayland,
  'Empty view': enable_empty_view,
  'gtk-doc': enable_gtk_doc,
  'Introspection': enable_introspection,
}, section: 'caja-' + package_version)
