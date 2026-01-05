app_name = 'ResEdit'

app_srcs = [
    'App.cpp',
    'BitmapView.cpp',
    'ImageEditor.cpp',
    'MiscEditors.cpp',
    'NumberEditors.cpp',
    'PreviewColumn.cpp',
    'ResFields.cpp',
    'ResListView.cpp',
    'ResourceData.cpp',
    'ResourceRoster.cpp',
    'ResView.cpp',
    'ResWindow.cpp',
]


be_app = executable(
    app_name,
    app_srcs,
    dependencies: [dep_libbe, dep_libtracker],
    include_directories: common_inc,
    install: true,
    link_args: is_windows ? ['-Wl,--disable-stdcall-fixup', '-Wl,--enable-auto-import', '-Wl,--no-insert-timestamp', '-static-libgcc', '-static-libstdc++', '-lmsvcrt'] : [],
    win_subsystem: is_windows ? 'windows' : 'console',
)
