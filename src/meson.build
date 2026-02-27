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

libwpeplatformgtk = shared_library(
  'wpeplatform-gtk',
  sources: libwpeplatformgtk_sources,
  dependencies: [ wpe_platform_dep, gtk_dep, epoxy_dep ],
  install: true,
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

