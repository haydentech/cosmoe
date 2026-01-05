app_name = 'guido'

app_srcs = [
    'diswindow.cpp',
    'disview.cpp',
    'disapp.cpp',
    'app.cpp',
]


be_app = executable(
    app_name,
    app_srcs,
    dependencies: [dep_libbe, dep_libtracker],
    include_directories: [common_inc, tracker_inc],
    install: true,
    link_args: is_windows ? ['-Wl,--disable-stdcall-fixup', '-Wl,--enable-auto-import', '-Wl,--no-insert-timestamp', '-static-libgcc', '-static-libstdc++', '-lmsvcrt'] : [],
    win_subsystem: is_windows ? 'windows' : 'console',
)

app_resource_defs = [
    app_name + '.rdef'
]

# AboutSystem has already moved the artwork to the build directory so the resource compiler can find it

app_resources = custom_target('app resources',
    depends : [be_app, rc_app],
    depend_files : meson.global_source_root() + '/data/artwork/Walter logo.png',
    input : app_resource_defs,
    output : app_name + '.rsrc',
    command : [rc_app, '@INPUT@', '-o', '@OUTPUT@', '-I', '.', '-I', meson.global_build_root() + '/data/artwork'],
    build_by_default : true)

custom_target('app plus resources',
    depends : app_resources,
    input : be_app,
    output : 'fake',
    command : [xres_app, '-o', '@INPUT@', app_resources],
    build_by_default : true)