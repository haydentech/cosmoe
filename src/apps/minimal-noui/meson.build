app_name = 'minimal-noui'

srcs_minimal_noui = [
    'app.cpp',
    'disapp.cpp',
]

app_resource_defs = [
    app_name + '.rdef',
]

# Compile .rdef to .rsrc
minimal_noui_resources = custom_target('minimal-noui resources',
    depends : [rc_app],
    depend_files : meson.global_source_root() + '/data/artwork/Walter logo.png',
    input : app_resource_defs,
    output : app_name + '.rsrc',
    command : [rc_app, '@INPUT@', '-o', '@OUTPUT@', '-I', '.', '-I', meson.global_build_root() + '/data/artwork'],
    build_by_default : true)

# Create a .o file with the resources embedded as a section
if is_darwin
    # macOS: Strip the first 4 bytes from .rsrc file
    minimal_noui_resources_stripped = custom_target('minimal-noui resources stripped',
        input : minimal_noui_resources,
        output : app_name + '_rsrc_data',
        command : ['dd', 'if=@INPUT@', 'of=@OUTPUT@', 'bs=4', 'skip=1', 'status=none'],
        build_by_default : true)
    
    minimal_noui_resources_stub = custom_target('minimal-noui resources stub',
        output : app_name + '_stub.o',
        command : [
            'sh', '-c',
            'echo "void _cosmoe_rsrc_stub_' + app_name.underscorify() + '(void) {}" | ' +
            cc.cmd_array()[0] + ' -x c - -c -o @OUTPUT@'
        ],
        build_by_default : true)
    
    minimal_noui_resources_obj = custom_target('minimal-noui resources object',
        input : minimal_noui_resources_stripped,
        output : app_name + '_rsrc.o',
        depends : minimal_noui_resources_stub,
        command : [
            'ld', '-r',
            minimal_noui_resources_stub,
            '-sectcreate', '__TEXT', '__rsrc', '@INPUT@',
            '-o', '@OUTPUT@'
        ],
        build_by_default : true)
elif is_linux
    minimal_noui_resources_obj = custom_target('minimal-noui resources object',
        input : minimal_noui_resources,
        output : app_name + '_rsrc.o',
        command : [
            'ld', '-r', '-b', 'binary',
            '@INPUT@',
            '-o', '@OUTPUT@'
        ],
        build_by_default : true)
else
    minimal_noui_resources_obj = []
endif

minimal_noui_app = executable(
    app_name,
    srcs_minimal_noui,
    objects: is_darwin or is_linux ? [minimal_noui_resources_obj] : [],
    link_with: [
        lib_libbe,
    ],
    dependencies: [
        dep_libbe
    ],
    include_directories: common_inc,
    install: true,
)