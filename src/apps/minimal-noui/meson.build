app_name = 'minimal-noui'

srcs_minimal_noui = [
    'app.cpp',
    'disapp.cpp',
]

app_resource_defs = [
    app_name + '.rdef',
]

# Compile .rdef to .rsrc
app_resources = custom_target('minimal-noui resources',
    depends : rc_app_dep,
    depend_files : meson.global_source_root() + '/data/artwork/Walter logo.png',
    input : app_resource_defs,
    output : app_name + '.rsrc',
    command : [rc_app, '@INPUT@', '-o', '@OUTPUT@', '-I', '.', '-I', meson.global_build_root() + '/data/artwork'],
    build_by_default : true)

# Create a .o file with the resources embedded as a section
if is_darwin
    # Step 1: Create empty C file and compile it
    app_resources_stub = custom_target('app resources stub',
        output : app_name + '_stub.o',
        command : [
            'sh', '-c',
            'echo "void _cosmoe_rsrc_stub_@0@(void) {}" | '.format(app_name.replace('-', '_')) +
            cc.cmd_array()[0] + ' -x c - -c -o @OUTPUT@'
        ],
        build_by_default : true)
    
    # Step 2: Use ld -r to add the resource section to the stub object
    app_resources_obj = custom_target(app_name + ' resources object',
        input : app_resources,
        output : app_name + '_rsrc.o',
        depends : app_resources_stub,
        command : [
            'ld', '-r',
            app_resources_stub,
            '-sectcreate', '__TEXT', '__rsrc', '@INPUT@',
            '-o', '@OUTPUT@'
        ],
        build_by_default : true)
elif is_linux
    # Linux: Use ld -r -b binary to create a .o file
    # Add .note.GNU-stack section to prevent executable stack warning
    app_resources_obj = custom_target(app_name + ' resources object',
        input : app_resources,
        output : app_name + '_rsrc.o',
        command : [
            'sh', '-c',
            # Create object with embedded binary (use basename so symbols don't include path)
            'INPUT_DIR=$(dirname "$1") && INPUT_FILE=$(basename "$1") && OUTPUT_ABS=$(cd $(dirname "$2") && pwd)/$(basename "$2") && ' +
            'cd "$INPUT_DIR" && ld -r -b binary "$INPUT_FILE" -o "$OUTPUT_ABS" && ' +
            # Add .note.GNU-stack section to mark stack as non-executable
            'objcopy --add-section .note.GNU-stack=/dev/null ' +
            '--set-section-flags .note.GNU-stack=noload,readonly ' +
            '"$OUTPUT_ABS"',
            '--', '@INPUT@', '@OUTPUT@'
        ],
        build_by_default : true)
else
    # Windows: Use objcopy to embed resources in PE/COFF format
    # objcopy creates symbols based on filename, so we rename them to a standard name
    app_resources_obj = custom_target(app_name + ' resources object',
        input : app_resources,
        output : app_name + '_rsrc.o',
        command : [
            'sh', '-c',
            'objcopy -I binary -O pe-x86-64 -B i386:x86-64 ' +
            '--rename-section .data=.cosmoe_rsrc ' +
            '"$1" "$2"',
            '--', '@INPUT@', '@OUTPUT@'
        ],
        build_by_default : true)
endif

minimal_noui_app = executable(
    app_name,
    srcs_minimal_noui,
    objects: [app_resources_obj],
    link_with: [
        lib_libbe,
    ],
    dependencies: [
        dep_libbe
    ],
    include_directories: common_inc,
    install: true,
)