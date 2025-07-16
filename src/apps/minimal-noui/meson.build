app_name = 'minimal-noui'

srcs_minimal_noui = [
    'app.cpp',
    'disapp.cpp',
]




minimal_noui_app = executable(
    app_name,
    srcs_minimal_noui,
    link_with: [
        lib_libbe,
    ],
    dependencies: [
        dep_wayland_client,
        dep_libbe
    ],
    include_directories: common_inc,
    install: true,
)

app_resource_defs = [
    app_name + '.rdef',
]

# AboutSystem has already moved the artwork to the build directory so the resource compiler can find it

minimal_noui_resources = custom_target('aboutsystem resources',
    depends : minimal_noui_app,
    depend_files : meson.global_source_root() + '/data/artwork/Walter logo.png',
    input : app_resource_defs,
    output : app_name + '.rsrc',
    command : [rc_app, '@INPUT@', '-o', '@OUTPUT@', '-I', '.', '-I', meson.global_build_root() + '/data/artwork'],
    build_by_default : true)

custom_target('minimal plus resources',
    depends : minimal_noui_resources,
    input : minimal_noui_app,
    output : 'fake',
    command : [xres_app, '-o', '@INPUT@', aboutsystem_resources],
    build_by_default : true)