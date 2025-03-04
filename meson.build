project(
  'wpe-platform-gtk', 'c',
  version: '0.0.1',
  license: 'LGPL2.1+',
  meson_version: '>= 0.60.0',
  default_options: ['c_std=c11']
)

prefix = get_option('prefix')
libdir = join_paths(prefix, get_option('libdir'))

wpe_platform_dep = dependency('wpe-platform-2.0', version: '>= 2.47.0')
gmodule_dep = dependency('gmodule-2.0', version: '>= 2.70.0')
gtk_dep = dependency('gtk4', version: '>= 4.16.0')
epoxy_dep = dependency('epoxy', version: '>= 1.4')

wpe_platform_module_dir = wpe_platform_dep.get_variable('moduledir', pkgconfig_define: ['libdir', join_paths(prefix, libdir)])

subdir('src')
subdir('demo')