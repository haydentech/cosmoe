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
    dependencies: dep_libbe,
    include_directories: common_inc,
    install: true,
)

app_resource_defs = [
    app_name + '.rdef'
]

app_resources = custom_target('app resources',
    depends : [be_app, rc_app],
    depend_files : meson.source_root() + '/data/artwork/Walter logo.png',
    input : app_resource_defs,
    output : app_name + '.rsrc',
    command : [rc_app, '@INPUT@', '-o', '@OUTPUT@', '-I', '.', '-I', meson.build_root() + '/data/artwork'],
    build_by_default : true)

custom_target('app plus resources',
    depends : app_resources,
    input : be_app,
    output : 'fake',
    command : [xres_app, '-o', '@INPUT@', app_resources],
    build_by_default : true)