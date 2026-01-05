app_name = 'minimal'

srcs_minimal = [
    'app.cpp',
    'disapp.cpp',
    'diswindow.cpp',
    'disview.cpp'
]

minimal_app = executable(
    app_name,
    srcs_minimal,
    link_with: [ lib_libbe ],
    dependencies: [ dep_libbe ],
    link_args: is_windows ? ['-Wl,--enable-runtime-pseudo-reloc', '-Wl,--enable-auto-import'] : [],
    include_directories: common_inc,
    win_subsystem: is_windows ? 'windows' : 'console',
    install: true,
)
