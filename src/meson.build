sources = files(
  'wpe-display-gtk.c',
  'wpe-drawing-area.c',
  'wpe-keymap-gtk.c',
  'wpe-monitor-gtk.c',
  'wpe-platform-gtk-module.c',
  'wpe-view-gtk.c'
)

module = shared_module(
  'giowpeplatform-gtk',
  sources: sources,
  dependencies: [wpe_platform_dep, gmodule_dep, gtk_dep],
  install: true,
  install_dir: wpe_platform_module_dir
)

