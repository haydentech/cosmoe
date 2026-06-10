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
subdir('hvif')
subdir('ppm')
subdir('tga')
subdir('ico')
subdir('stxt')

# Only build ICNS translator if libicns is available
if dep_libicns.found()
	subdir('icns')
endif

# Only build RAW translator if libraw is available
if dep_raw.found()
	subdir('raw')
endif

# Only build JPEG translator if libjpeg is available
if dep_libjpeg.found()
	subdir('jpeg')
endif

#subdir('ppm')	#PPM translator is not currently valid in Haiku

# Only build WebP translator if libwebp is available
if dep_webp.found()
	subdir('webp')
endif

# Only build TIFF translator if libtiff is available
if dep_libtiff.found()
	subdir('tiff')
endif
