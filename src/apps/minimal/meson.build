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
    link_with: [
        lib_libbe,
        lib_libtracker ],
    dependencies: [
        dep_libbe,
        dep_libtracker
    ],
    include_directories: common_inc,
    install: true,
)

srcs_minimal_resources = [
    app_name + '.rdef'
]

# AboutSystem has already moved the artwork to the build directory so the resource compiler can find it

minimal_resources = custom_target('minimal resources',
    depends : minimal_app,
    input : srcs_minimal_resources,
    output : app_name + '.rsrc',
    command : [rc_app, '@INPUT@', '-o', '@OUTPUT@', '-I', '.'],
    build_by_default : true)

custom_target('minimal plus resources',
    depends : minimal_resources,
    input : minimal_app,
    output : 'fake',
    command : [xres_app, '-o', '@INPUT@', aboutsystem_resources],
    build_by_default : true)