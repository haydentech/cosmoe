# X11 backend plugin for Cosmoe

x11_sources = files([
    'window.c',
])

x11_backend_sources = files([
    'X11Backend.cpp',
])

# Check for XRandR support
xrandr_dep = dependency('xrandr', required: false)
x11_backend_cflags = []
if xrandr_dep.found()
    x11_backend_cflags += ['-DHAVE_XRANDR']
endif

x11_backend = shared_module(
    'cosmoe-x11',
    [x11_backend_sources, x11_sources],
    include_directories: [include_directories('.'), common_inc],
    dependencies: [
        dependency('x11'),
        dependency('x11-xcb'),
        dependency('xkbcommon'),
        dependency('xkbcommon-x11'),
        dependency('cairo'),
        dependency('cairo-xlib'),
        xrandr_dep,
    ],
    c_args: x11_backend_cflags,
    cpp_args: x11_backend_cflags,
    install: true,
    install_dir: '/usr/local/lib',
    name_prefix: 'lib',
)
