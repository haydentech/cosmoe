srcs_minimal_noui = [
    'app.cpp',
    'disapp.cpp',
]


minimal_noui_app = executable(
    'minimal-noui',
    srcs_minimal_noui,
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
