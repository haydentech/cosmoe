# Wayland backend plugin for Cosmoe

wayland_backend_sources = files([
    '../WaylandBackend.cpp',
])

wayland_backend = shared_module(
    'cosmoe-wayland',
    [wayland_backend_sources, srcs_wayland],
    include_directories: [common_inc],
    dependencies: [
        dependency('wayland-client'),
        dependency('wayland-cursor'),
        dependency('xkbcommon'),
        dependency('cairo'),
        dependency('libpng'),
        dependency('libjpeg'),
        dependency('libwebp'),
        dependency('pango'),
        dependency('pangocairo'),
    ],
    install: true,
    install_dir: '/usr/local/lib',
    name_prefix: 'lib',
)
