control_look_name = 'FlatControlLook'

srcs_control_look = files(
    'FlatControlLook.cpp',
    '../ControlLookInit.cpp',
)

lib_control_look = shared_library(
	control_look_name,
    srcs_control_look,
    name_prefix: '',
    include_directories: [common_inc],
    link_with: [
        lib_libbe,
     ],
	dependencies: deps_libbe,
    install_dir: control_look_dir,
	install: false,
)

srcs_control_look_resources = [
    control_look_name + '.rdef'
]

control_look_resources = custom_target('control look resources',
    depends : [lib_control_look, rc_app],
    input : srcs_control_look_resources,
    output : control_look_name + '.rsrc',
    command : [rc_app, '@INPUT@', '-o', '@OUTPUT@', '-I', '.'],
    build_by_default : true)

custom_target('control look plus resources',
    depends : control_look_resources,
    input : lib_control_look,
    output : 'fake',
    command : [xres_app, '-o', '@INPUT@', control_look_resources],
    build_by_default : true)