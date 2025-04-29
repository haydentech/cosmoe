translator_name = 'BMPTranslator'

srcs_translator = files(
    'BMPMain.cpp',
    'BMPTranslator.cpp',
    'BMPView.cpp',
)

lib_translator = executable(
	translator_name,
	srcs_translator,
	include_directories: [common_inc, '../shared'],
    link_with: [
        lib_libbe,
        lib_libroot,
    ],
	dependencies: deps_libbe,
    link_whole: [lib_translatorsutils],
	install: true,
)

srcs_translator_resources = [
    translator_name + '.rdef'
]

translator_resources = custom_target('translator resources',
    depends : lib_translator,
    input : srcs_translator_resources,
    output : translator_name + '.rsrc',
    command : ['rc', '@INPUT@', '-o', '@OUTPUT@', '-I', '.'],
    build_by_default : true)

custom_target('translator plus resources',
    depends : translator_resources,
    input : lib_translator,
    output : 'fake',
    command : ['xres', '-o', '@INPUT@', translator_resources],
    build_by_default : true)