translator_name = 'PNGTranslator'

srcs_translator = files(
    'PNGMain.cpp',
    'PNGTranslator.cpp',
    'PNGView.cpp',
)

lib_translator = shared_library(
	translator_name,
    srcs_translator,
    link_args: ['-Wl,-e,main'], # Allows the shared library to be used as an executable too
    name_prefix: '',
    include_directories: [common_inc, '../shared'],
    link_with: [
        lib_libbe,
        lib_libroot,
    ],
	dependencies: deps_libbe,
    link_whole: [lib_translatorsutils],
    install_dir: '/usr/local/lib/translators',
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