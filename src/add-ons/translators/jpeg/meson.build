translator_name = 'JPEGTranslator'

srcs_translator = files(
    'be_jdatadst.cpp',
    'be_jdatasrc.cpp',
    'be_jerror.cpp',
    'exif_parser.cpp',
    'JPEGTranslator.cpp',
)


# On Linux, the -e flag allows the shared library to be used as an executable too
# On macOS (Mach-O), this flag doesn't apply and causes linker warnings
link_args_translator = host_machine.system() == 'linux' ? ['-Wl,-e,main'] : []

lib_translator = shared_library(
	translator_name,
    srcs_translator,
    link_args: link_args_translator,
    name_prefix: '',
    include_directories: [common_inc, '../shared', '../raw'],
    link_with: [
        lib_libbe,
    ],
	dependencies: [deps_libbe, dep_libjpeg],
    link_whole: [lib_translatorsutils],
    install_dir: translator_dir,
	install: true,
)

srcs_translator_resources = [
    translator_name + '.rdef'
]

translator_resources = custom_target('translator resources',
    depends : [lib_translator, rc_app],
    input : srcs_translator_resources,
    output : translator_name + '.rsrc',
    command : [rc_app, '@INPUT@', '-o', '@OUTPUT@', '-I', '.'],
    build_by_default : true)

custom_target('translator plus resources',
    depends : translator_resources,
    input : lib_translator,
    output : 'fake',
    command : [xres_app, '-o', '@INPUT@', translator_resources],
    build_by_default : true)