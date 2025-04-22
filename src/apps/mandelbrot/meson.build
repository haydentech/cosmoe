srcs_mandelbrot = [
    'FractalEngine.cpp',
    'Mandelbrot.cpp',
]


executable(
    'Mandelbrot',
    srcs_mandelbrot,
    link_with: [
        lib_libbe,
        lib_libroot,
    ],
    dependencies: [
        dep_wayland_client,
        dep_libroot,
        dep_libbe
    ],
    include_directories: common_inc,
    install: true,
)