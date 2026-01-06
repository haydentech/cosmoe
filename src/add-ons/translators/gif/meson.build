translator_name = 'GIFTranslator'

srcs_translator = files(
    'GIFLoad.cpp',
	'GIFSave.cpp',
	'GIFView.cpp',
    'GIFTranslator.cpp',
    'LoadPalette.cpp',
    'SavePalette.cpp',
	'SFHash.cpp',
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
    depends : [rc_app],
    input : srcs_translator_resources,
    output : translator_name + '.rsrc',
    command : [rc_app, '@INPUT@', '-o', '@OUTPUT@', '-I', '.'],
    build_by_default : true)

# Create a .o file with the resources embedded as a section
if is_darwin
    # macOS: Strip the first 4 bytes from .rsrc file
    translator_resources_stripped = custom_target('gif resources stripped',
        input : translator_resources,
        output : 'gif_rsrc_data',
        command : ['dd', 'if=@INPUT@', 'of=@OUTPUT@', 'bs=4', 'skip=1', 'status=none'],
        build_by_default : true)
    
    translator_resources_stub = custom_target('gif resources stub',
        output : 'gif_stub.o',
        command : [
            'sh', '-c',
            'echo "void _cosmoe_rsrc_stub_gif(void) {}" | ' +
            cc.cmd_array()[0] + ' -x c - -c -o @OUTPUT@'
        ],
        build_by_default : true)
    
    translator_resources_obj = custom_target('gif resources object',
        input : translator_resources_stripped,
        output : 'gif_rsrc.o',
        depends : translator_resources_stub,
        command : [
            'ld', '-r',
            translator_resources_stub,
            '-sectcreate', '__TEXT', '__rsrc', '@INPUT@',
            '-o', '@OUTPUT@'
        ],
        build_by_default : true)
elif is_linux
    translator_resources_obj = custom_target('gif resources object',
        input : translator_resources,
        output : 'gif_rsrc.o',
        command : [
            'ld', '-r', '-b', 'binary',
            '@INPUT@',
            '-o', '@OUTPUT@'
        ],
        build_by_default : true)
else
    translator_resources_obj = []
endif