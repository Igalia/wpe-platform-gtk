libwpeplatformgtk_header_subdir = 'wpe-platform-gtk-@0@'.format(api_version)

libwpeplatformgtk_public_headers = files(
  'wpe-display-gtk.h',
  'wpe-toplevel-gtk.h',
  'wpe-view-gtk.h',
  'wpe-platform-gtk-version.h'
)

install_headers(
  libwpeplatformgtk_public_headers,
  subdir: libwpeplatformgtk_header_subdir
)

libwpeplatformgtk_sources = files(
  'wpe-clipboard-gtk.c',
  'wpe-display-gtk.c',
  'wpe-drawing-area.c',
  'wpe-input-method-context-gtk.c',
  'wpe-keymap-gtk.c',
  'wpe-screen-gtk.c',
  'wpe-toplevel-gtk.c',
  'wpe-view-gtk.c'
)

config_data = configuration_data()

config_data.set('_WPE_PLATFORM_GTK_EXTERN', '__attribute__((visibility("default"))) extern')

configure_file(
  output: 'config.h',
  configuration: config_data
)

libwpeplatformgtk = shared_library(
  'wpeplatform-gtk',
  sources: libwpeplatformgtk_sources,
  dependencies: [ wpe_platform_dep, gtk_dep, epoxy_dep ],
  gnu_symbol_visibility: 'hidden',
  install: true
)

libwpeplatformgtk_dep = declare_dependency(link_with : libwpeplatformgtk,
  dependencies : [ wpe_platform_dep, gtk_dep ],
)

giowpeplatformgtk_sources = files(
  'wpe-platform-gtk-module.c'
)

giowpeplatformgtk = shared_module(
  'giowpeplatform-gtk',
  sources: giowpeplatformgtk_sources,
  dependencies: [ libwpeplatformgtk_dep ],
  install: true,
  install_dir: wpe_platform_module_dir
)

pkg = import('pkgconfig')

pkg.generate(
  libraries: libwpeplatformgtk,
  subdirs: [ libwpeplatformgtk_header_subdir ],
  name: 'WPE GTK Platform',
  filebase: package_api_name,
  description: 'GTK Platform implementation for WPE WebKit',
  version: api_version,
  requires: [ wpe_platform_dep, gtk_dep ]
)
