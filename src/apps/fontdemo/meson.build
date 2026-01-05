srcs_fontdemo = [
    'ControlView.cpp',
    'FontDemo.cpp',
    'FontDemoView.cpp',
]


fontdemo_app = executable(
    'FontDemo',
    srcs_fontdemo,
    link_with: [
        lib_libbe,
    ],
    dependencies: [
        dep_libbe
    ],
    include_directories: common_inc,
    install: true,
    link_args: is_windows ? ['-Wl,--disable-stdcall-fixup', '-Wl,--enable-auto-import', '-Wl,--no-insert-timestamp', '-static-libgcc', '-static-libstdc++', '-lmsvcrt'] : [],
    win_subsystem: is_windows ? 'windows' : 'console',
)
