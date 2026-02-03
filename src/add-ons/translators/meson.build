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

translator_dir = dir_lib / 'addons/Translators'

subdir('png')
subdir('bmp')
subdir('gif')

# Only build JPEG translator if libjpeg is available
if dep_libjpeg.found()
	subdir('jpeg')
endif

#subdir('ppm')	#PPM translator is not currently valid in Haiku

# Only build WebP translator if libwebp is available
if dep_webp.found()
	subdir('webp')
endif
