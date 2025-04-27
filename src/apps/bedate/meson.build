srcs_bedate = [
    'app.cpp',
    'disapp.cpp',
    'diswindow.cpp',
    'disview.cpp',
    'julian.cpp',
    'discordian.cpp',
]


bedate_app = executable(
    'bedate',
    srcs_bedate,
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
