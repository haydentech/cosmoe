src_translatorsutils = files(
    'shared/BaseTranslator.cpp',
    'shared/StreamBuffer.cpp',
    'shared/TranslatorSettings.cpp',
    'shared/TranslatorWindow.cpp',
)

lib_translatorsutils = static_library(
	'translatorsutils',
	src_translatorsutils,
	include_directories: common_inc,
	dependencies: deps_libbe,
	install: false,
)

translator_dir = '/usr/local/lib/addons/Translators'

subdir('png')
subdir('bmp')
subdir('jpeg')
