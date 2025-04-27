srcs_guido = [
    'diswindow.cpp',
    'disview.cpp',
    'disapp.cpp',
    'app.cpp',
]


guido_app = executable(
    'guido',
    srcs_guido,
    dependencies: dep_libbe,
    include_directories: common_inc,
    install: true,
)

srcs_guido_resources = [
    'guido.rdef'
]

guido_resources = custom_target('guido resources',
    depends : guido_app,
    input : srcs_guido_resources,
    output : 'guido.rsrc',
    command : ['rc', '@INPUT@', '-o', '@OUTPUT@', '-I', '.'],
    build_by_default : true)

custom_target('guido plus resources',
    depends : guido_resources,
    input : guido_app,
    output : 'fake',
    command : ['xres', '-o', '@INPUT@', guido_resources],
    build_by_default : true)