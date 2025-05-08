srcs_minimal = [
    'app.cpp',
    'disapp.cpp',
    'diswindow.cpp',
    'disview.cpp'
]


minimal_app = executable(
    'minimal',
    srcs_minimal,
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
