control_look_name = 'BeControlLook'

srcs_control_look = files(
    'BeControlLook.cpp',
    '../ControlLookInit.cpp',
)

srcs_control_look_resources = [
    control_look_name + '.rdef'
]

control_look_resources = custom_target('control look resources',
    depends : rc_app_dep,
    input : srcs_control_look_resources,
    output : control_look_name + '.rsrc',
    command : [rc_app, '@INPUT@', '-o', '@OUTPUT@', '-I', '.'],
    build_by_default : true)

# Create a .o file with the resources embedded as a section
if is_darwin
    control_look_resources_stub = custom_target('BeControlLook resources stub',
        output : 'BeControlLook_stub.o',
        command : [
            'sh', '-c',
            'echo "void _cosmoe_rsrc_stub_BeControlLook(void) {}" | ' +
            cc.cmd_array()[0] + ' -x c - -c -o @OUTPUT@'
        ],
        build_by_default : true)
    
    control_look_resources_obj = custom_target('BeControlLook resources object',
        input : control_look_resources,
        output : 'BeControlLook_rsrc.o',
        depends : control_look_resources_stub,
        command : [
            'ld', '-r',
            control_look_resources_stub,
            '-sectcreate', '__TEXT', '__rsrc', '@INPUT@',
            '-o', '@OUTPUT@'
        ],
        build_by_default : true)
elif is_linux
    control_look_resources_obj = custom_target('BeControlLook resources object',
        input : control_look_resources,
        output : 'BeControlLook_rsrc.o',
        command : [
            'sh', '-c',
            # Create object with embedded binary
            'ld -r -b binary "$1" -o "$2" && ' +
            # Add .note.GNU-stack section to prevent executable stack warning
            'objcopy --add-section .note.GNU-stack=/dev/null ' +
            '--set-section-flags .note.GNU-stack=noload,readonly "$2"',
            '--', '@INPUT@', '@OUTPUT@'
        ],
        build_by_default : true)
else
    control_look_resources_obj = []
endif

lib_control_look = shared_library(
	control_look_name,
    srcs_control_look,
    objects: is_darwin or is_linux ? [control_look_resources_obj] : [],
    name_prefix: '',
    include_directories: [common_inc],
    link_with: [
        lib_libbe,
     ],
	dependencies: deps_libbe,
    install_dir: control_look_dir,
	install: false,
)