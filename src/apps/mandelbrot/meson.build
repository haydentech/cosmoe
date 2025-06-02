srcs_mandelbrot = [
    'FractalEngine.cpp',
    'Mandelbrot.cpp',
]


mandelbrot_app = executable(
    'Mandelbrot',
    srcs_mandelbrot,
    link_with: [
        lib_libbe,
        lib_libtracker,
    ],
    dependencies: [
        dep_wayland_client,
        dep_libbe,
        dep_libtracker,
    ],
    include_directories: common_inc,
    install: true,
)

srcs_mandelbrot_resources = [
    'Mandelbrot.rdef'
]

mandelbrot_resources = custom_target('mandelbrot resources',
    depends : [mandelbrot_app, rc_app],
    input : srcs_mandelbrot_resources,
    output : 'mandelbrot.rsrc',
    command : [rc_app, '@INPUT@', '-o', '@OUTPUT@', '-I', '.'],
    build_by_default : true)

custom_target('mandelbrot plus resources',
    depends : mandelbrot_resources,
    input : mandelbrot_app,
    output : 'fake',
    command : [xres_app, '-o', '@INPUT@', mandelbrot_resources],
    build_by_default : true)