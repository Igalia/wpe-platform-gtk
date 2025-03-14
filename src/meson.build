sources = files(
  'wpe-display-gtk.c',
  'wpe-drawing-area.c',
  'wpe-input-method-context-gtk.c',
  'wpe-keymap-gtk.c',
  'wpe-platform-gtk-module.c',
  'wpe-screen-gtk.c',
  'wpe-toplevel-gtk.c',
  'wpe-view-gtk.c'
)

module = shared_module(
  'giowpeplatform-gtk',
  sources: sources,
  dependencies: [wpe_platform_dep, gmodule_dep, gtk_dep],
  install: true,
  install_dir: wpe_platform_module_dir
)

