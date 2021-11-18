project('mauncher', ['c'])

libm = meson.get_compiler('c').find_library('m', required: false)
gtk_layer_shell = dependency(
  ['gtk-layer-shell', 'gtk-layer-shell-0'],
  fallback: ['gtk-layer-shell', 'gtk_layer_shell_dep'],
)
threads = dependency('threads')
gtk3 = dependency('gtk+-3.0')
wayland_client = dependency('wayland-client')

mauncher = executable(
  'mauncher',
  sources: [
    'mauncher.c',
    'mauncher-win.c',
    'mauncher-ipc.c',
    'sysutil.c',
  ],
  dependencies: [gtk_layer_shell, threads, libm, wayland_client, gtk3],
  install: true,
)
launcher = executable(
  'mauncher-launcher',
  sources: [
    'mauncher-launcher.c',
    'sysutil.c',
  ],
  dependencies: [gtk_layer_shell, threads, libm, gtk3, wayland_client],
  install: true,
)
