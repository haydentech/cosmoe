project('cosmoe',
	['c', 'cpp'],
	version: '0.9.0',
  license: 'MIT',
)

version_cosmoe = meson.project_version()

dir_prefix = get_option('prefix')
dir_bin = dir_prefix / get_option('bindir')
dir_data = dir_prefix / get_option('datadir')


cc = meson.get_compiler('c')
c_args = cc.get_supported_arguments(
	'-Wall',
	'-Wpointer-arith',
  '-Wcast-align',
	'-Wsign-compare',
	'-Wno-multichar',
	'-fPIC',
)
add_project_arguments(c_args, language: 'c')


cpp = meson.get_compiler('cpp')
cpp_args = cpp.get_supported_arguments(
	'-Woverloaded-virtual',
  '-Wno-ctor-dtor-privacy',
  '-Wnon-virtual-dtor',
  '-fexceptions',
)
add_project_arguments([c_args, cpp_args], language: 'cpp')


public_inc = include_directories([
  'build/config_headers',
  'headers',
  'headers/libs/agg',
  'headers/libs/icon',
  'headers/libs/linprog',
  'headers/libs/mapm',
  'headers/libs/wayland',
  'headers/libs/wayland/shared',
  'headers/libs/wayland/protocol',
  'headers/os/add-ons/registrar',
  'headers/os',
  'headers/os/interface',
  'headers/os/support',
  'headers/os/kernel',
  'headers/os/storage',
  'headers/os/app',
  'headers/os/device',
  'headers/os/locale',
  'headers/os/translation',
  'headers/os/drivers',
  'headers/os/game',
  'headers/os/package',
  'headers/os/add-ons/file_system',
  'headers/posix',
  'headers/private/app',
  'headers/private/shared',
  'headers/private/interface',
  'headers/private/locale',
  'headers/private/mount',
  'headers/private/storage',
  'headers/private/system',
  'headers/os/add-ons/graphics',
  'src/kits/tracker',
  'src/kits/tracker/infowindow',
  'src/kits/interface/textview_support',
  'src/kits/interface/layouter',
  'src/kits/storage/disk_device',
  'src/kits/storage/disk_device/jobs',
  'headers/private',
  'headers/private/input',
  'headers/private/graphics',
  'headers/private/tracker',
  'headers/private/kernel',
  'headers/private/kernel/util',
  'headers/private/libroot',
  'headers/private/support',
  'headers/private/print',
  'headers/private/textencoding',
  'src/libs/icon/shape',
  'src/libs/icon/flat_icon',
  'src/libs/icon/message'
])
common_inc = [ include_directories('.'), public_inc ]

agg_inc = include_directories([
  'headers/libs/agg',
  'headers/os/support'
])

config_h = configuration_data()

check_headers = [
  'dirent.h',
  'inttypes.h',
  'ndir.h',
  'stdint.h',
  'stdio.h',
  'stdlib.h',
  'strings.h',
  'string.h',
  'sys/dir.h',
  'sys/ndir.h',
  'sys/stat.h',
  'sys/types.h',
  'unistd.h',
  'sys/xattr.h'
]

foreach h : check_headers
  if cpp.has_header(h)
    config_h.set('HAVE_' + h.underscorify().to_upper(), 1)
  endif
endforeach

optional_libc_funcs = [
  'strlcat',
  'strlcpy',
	'mkostemp',
  'strchrnul',
  'initgroups',
  'posix_fallocate',
	'memfd_create',
  'unreachable',
]

foreach func : optional_libc_funcs
	if cc.has_function(func)
		config_h.set('HAVE_' + func.to_upper(), 1)
	endif
endforeach

optional_builtins = {
	'builtin_clz': 'return __builtin_clz(1);',
	'builtin_bswap32': 'return __builtin_bswap32(0);',
	'builtin_popcount': 'return __builtin_popcount(0);',
}
foreach name, check : optional_builtins
	if cc.links('int main(void) { @0@ }'.format(check), name: name)
		config_h.set('HAVE_' + name.to_upper(), 1)
	endif
endforeach

config_h.set('_GNU_SOURCE', '1')
config_h.set('_ALL_SOURCE', '1')
config_h.set('USE_RESIZE_POOL', '1')

config_h.set_quoted('PACKAGE_STRING', 'cosmoe @0@'.format(version_cosmoe))
config_h.set_quoted('PACKAGE_VERSION', version_cosmoe)
config_h.set_quoted('VERSION', version_cosmoe)
config_h.set_quoted('PACKAGE_URL', 'https://cosmoe.org')
#config_h.set_quoted('PACKAGE_BUGREPORT', 'https://gitlab.freedesktop.org/wayland/weston/issues/')

config_h.set_quoted('BINDIR', dir_bin)
config_h.set_quoted('DATADIR', dir_data)

dep_xkbcommon = dependency('xkbcommon', version: '>= 0.3.0')
if dep_xkbcommon.version().version_compare('>= 0.5.0')
	config_h.set('HAVE_XKBCOMMON_COMPOSE', '1')
endif

dep_wayland_client = dependency('wayland-client', version: '>= 1.22.0')
dep_pixman = dependency('pixman-1', version: '>= 0.25.2')
dep_libm = cc.find_library('m')
dep_libdl = cc.find_library('dl')

dep_pango = dependency('pango')
dep_pangocairo = dependency('pangocairo')
dep_fontconfig = dependency('fontconfig')
dep_glib = dependency('glib-2.0', version: '>= 2.36')

dep_icu = dependency('icu-i18n', version: '>= 74.0')

deps_cairo_shared = [
	dependency('cairo'),
	dependency('libpng'),
	dep_pixman,
	dep_libm,
]

if dep_pango.found() and dep_pangocairo.found() and dep_fontconfig.found() and dep_glib.found()
	deps_cairo_shared += [ dep_pango, dep_pangocairo, dep_fontconfig, dep_glib ]
	config_h.set('HAVE_PANGO', '1')
endif

dep_libjpeg = dependency('libjpeg')
if not dep_libjpeg.found()
  dep_libjpeg = cc.find_library('jpeg')
endif
if not dep_libjpeg.found()
  error('JPEG image loading requires libjpeg or jpeg, neither was found.')
endif
deps_cairo_shared += dep_libjpeg
config_h.set('HAVE_JPEG', '1')

dep_webp = dependency('libwebp')
if not dep_webp.found()
  error('WEBP image loading requires libwebp which was not found.')
endif
deps_cairo_shared += dep_webp
config_h.set('HAVE_WEBP', '1')

subdir('src/libs/mapm')
subdir('src/libs/linprog')
subdir('src/libs/agg')
subdir('src/libs/icon')
subdir('src/system')    # libroot
subdir('src/kits')      # libbe

subdir('src/apps')


configure_file(input : 'config.h.meson',
  output : 'config.h',
  configuration : config_h)
