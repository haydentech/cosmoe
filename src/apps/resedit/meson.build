app_name = 'ResEdit'
linux_desktop_id = 'x-vnd.Haiku-ResEdit'
linux_desktop_categories = 'Development;Utility;'
if is_linux
    meson.add_install_script(
        'sh', linux_desktop_entry_install_script,
        get_option('prefix'),
        linux_desktop_id,
        app_name,
        app_name,
        app_name.to_lower(),
        linux_desktop_categories)
endif

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


executable(
    app_name,
    app_srcs,
    dependencies: [dep_libbe, dep_libtracker],
    include_directories: common_inc,
    install: true,
    link_args: is_windows ? ['-Wl,--disable-stdcall-fixup', '-Wl,--enable-auto-import', '-Wl,--no-insert-timestamp', '-static-libgcc', '-static-libstdc++', '-lmsvcrt'] : [],
    win_subsystem: is_windows ? 'windows' : 'console',
)

if is_linux
    meson.add_install_script(
        'sh', mimeset_install_script,
        meson.global_build_root() / 'mimeset',
        meson.global_build_root(),
        dir_bin,
        app_name)
endif
