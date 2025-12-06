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
)
