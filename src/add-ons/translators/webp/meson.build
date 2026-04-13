translator_name = 'WebPTranslator'

srcs_translator = files(
    'main.cpp',
    'WebPTranslator.cpp',
    'ConfigView.cpp',
    '../TranslatorInit.cpp',
)

# On Linux, the -e flag allows the shared library to be used as an executable too
# On macOS (Mach-O), this flag doesn't apply and causes linker warnings
link_args_translator = host_machine.system() == 'linux' ? ['-Wl,-e,main'] : []


lib_translator = shared_library(
	translator_name,
    srcs_translator,
    objects: is_darwin or is_linux ? [translator_resources_obj] : [],
    link_args: link_args_translator,
    name_prefix: '',
    include_directories: [common_inc, '../shared'],
    link_with: [
        lib_libbe,
        lib_localestub,
     ],
	dependencies: deps_libbe,
    link_whole: [lib_translatorsutils],
    install_dir: translator_dir,
	install: true,
)

srcs_translator_resources = [
    translator_name + '.rdef'
]

translator_resources = custom_target('translator resources',
    depends : rc_app_dep,
    input : srcs_translator_resources,
    output : translator_name + '.rsrc',
    command : [rc_app, '@INPUT@', '-o', '@OUTPUT@', '-I', '.'],
    build_by_default : true)

# Create a .o file with the resources embedded as a section
if is_darwin
    translator_resources_stub = custom_target('webp resources stub',
        output : 'webp_stub.o',
        command : [
            'sh', '-c',
            'echo "void _cosmoe_rsrc_stub_webp(void) {}" | ' +
            cc.cmd_array()[0] + ' -x c - -c -o @OUTPUT@'
        ],
        build_by_default : true)
    
    translator_resources_obj = custom_target('webp resources object',
        input : translator_resources,
        output : 'webp_rsrc.o',
        depends : translator_resources_stub,
        command : [
            'ld', '-r',
            translator_resources_stub,
            '-sectcreate', '__TEXT', '__rsrc', '@INPUT@',
            '-o', '@OUTPUT@'
        ],
        build_by_default : true)
elif is_linux
    translator_resources_obj = custom_target('webp resources object',
        input : translator_resources,
        output : 'webp_rsrc.o',
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
    translator_resources_obj = []
endif