app_name = 'gui-torture'

srcs_gui_torture = [
    'app.cpp',
    'disapp.cpp',
    'diswindow.cpp',
    'disview.cpp'
]


gui_torture_app = executable(
    app_name,
    srcs_gui_torture,
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

srcs_gui_torture_resources = [
    app_name + '.rdef'
]

# AboutSystem has already moved the artwork to the build directory so the resource compiler can find it

gui_torture_resources = custom_target('gui-torture resources',
    depends : gui_torture_app,
    input : srcs_gui_torture_resources,
    output : app_name + '.rsrc',
    command : [rc_app, '@INPUT@', '-o', '@OUTPUT@', '-I', '.'],
    build_by_default : true)

custom_target('gui-torture plus resources',
    depends : gui_torture_resources,
    input : gui_torture_app,
    output : 'fake',
    command : [xres_app, '-o', '@INPUT@', aboutsystem_resources],
    build_by_default : true)