demo_sources = files(
  'main.c',
  'wg-tab-view.c',
  'wg-window.c'
)

wpe_webkit_dep = dependency('wpe-webkit-2.0', version: '>= 2.47.0')
adw_dep	= dependency('libadwaita-1', version: '>= 1.6')

executable('wpe-gtk-demo',
  sources: demo_sources,
  dependencies: [ libwpeplatformgtk_dep, wpe_webkit_dep, adw_dep ],
  include_directories: [ '.', '../src' ]
)