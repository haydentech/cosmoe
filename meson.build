project('cosmoe',
	['c', 'cpp'],
	version: '0.9.0',
	license: 'MIT',
	default_options: ['cpp_std=c++17']
)

version_cosmoe = meson.project_version()

dir_prefix = get_option('prefix')
dir_bin = dir_prefix / get_option('bindir')
dir_data = dir_prefix / get_option('datadir')

# Platform detection
# When cross-compiling, check target_machine, otherwise use host_machine
build_system = meson.is_cross_build() ? target_machine.system() : host_machine.system()
is_linux = build_system == 'linux'
is_darwin = build_system == 'darwin'
# MSYS2 reports system as 'msys', 'cygwin', or similar - treat as Windows
is_windows = build_system == 'windows' or build_system.startswith('msys') or build_system == 'cygwin'

# Add Objective-C/C++ support on macOS for Cocoa backend
if is_darwin
	add_languages('objc', 'objcpp', required: true)
endif

# Backend selection based on platform
# On Linux: Build X11 and optionally Wayland
# On macOS: Build only Cocoa backend
# On Windows: Build only Windows backend
if is_windows
	windows_enabled = true
	cocoa_enabled = false
	wayland_enabled = false
	x11_enabled = false
	message('Windows detected: Building Windows backend only')
elif is_darwin
	windows_enabled = false
	cocoa_enabled = true
	wayland_enabled = false
	x11_enabled = false
	message('macOS detected: Building Cocoa backend only')
else
	windows_enabled = false
	cocoa_enabled = false
	wayland_enabled = get_option('enable_wayland_backend')
	x11_enabled = get_option('enable_x11_backend')
	message('Linux detected: Building X11 and Wayland backends')
endif

unit_tests_enabled = get_option('enable_unit_test_compile')


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
  '-fexceptions',
)
# Disable non-virtual-dtor warning on Windows due to Windows SDK headers
if not is_windows
	cpp_args += cpp.get_supported_arguments('-Wnon-virtual-dtor')
endif
add_project_arguments([c_args, cpp_args], language: 'cpp')

# On macOS, also set up Objective-C and Objective-C++ compiler args
if is_darwin
	objc = meson.get_compiler('objc')
	add_project_arguments(c_args, language: 'objc')
	
	objcpp = meson.get_compiler('objcpp')
	add_project_arguments([c_args, cpp_args], language: 'objcpp')
endif

# On Windows/MSYS2, ensure _WIN32 is defined for MinGW headers
if is_windows
	add_project_arguments(['-D_WIN32', '-DWIN32'], language: 'c')
	add_project_arguments(['-D_WIN32', '-DWIN32'], language: 'cpp')
endif


public_inc_entries = [
  'build/config_headers',
  'headers',
  'headers/libs/agg',
  'headers/libs/icon',
  'headers/libs/mapm',
  # Wayland headers are included only when the Wayland backend is enabled
  # We'll append them to the list when wayland_enabled is true
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
  'headers/private/app',
  'headers/private/shared',
  'headers/private/interface',
  'headers/private/locale',
  'headers/private/mount',
  'headers/private/storage',
  'headers/private/system',
  'headers/os/add-ons/graphics',
  'src/kits/interface/textview_support',
  'src/kits/interface/layouter',
  'src/kits/storage/disk_device',
  'src/kits/storage/disk_device/jobs',
  'headers/libs/wayland/stubs',
  'headers/private',
  'headers/private/input',
  'headers/private/graphics',
  'headers/private/kernel',
  'headers/private/kernel/util',
  'headers/private/libroot',
  'headers/private/support',
  'headers/private/print',
  'headers/private/textencoding'
]
if wayland_enabled
  public_inc_entries += [
    'headers/libs/wayland',
	  'headers/libs/wayland/shared',
    'headers/libs/wayland/protocol',
  ]
endif

# On non-Windows platforms, add POSIX compat headers
if not is_windows
  public_inc_entries += ['headers/posix']
endif

public_inc = include_directories(public_inc_entries)
common_inc = [ include_directories('.'), public_inc ]

tracker_inc = include_directories([
  'src/kits/tracker',
  'src/kits/tracker/infowindow',
  'headers/private/tracker',
])

icon_inc = include_directories([
#  'src/libs/icon/shape',
  'src/libs/icon/flat_icon',
  'src/libs/icon/message',
  'src/libs/icon/generic',
  'src/libs/icon/style',
  'src/libs/icon/transformable',
  'src/libs/icon/transformer',
  'src/libs/icon',
])

agg_inc = include_directories([
  'headers/libs/agg',
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
config_h.set_quoted('PACKAGE_BUGREPORT', 'https://gitlab.com/haydentech/cosmoe/-/issues')

config_h.set_quoted('BINDIR', dir_bin)
config_h.set_quoted('DATADIR', dir_data)

# Platform-specific dependencies
# xkbcommon is only needed on Linux (X11/Wayland)
if is_linux
	dep_xkbcommon = dependency('xkbcommon', version: '>= 0.3.0')
	if dep_xkbcommon.version().version_compare('>= 0.5.0')
		config_h.set('HAVE_XKBCOMMON_COMPOSE', '1')
	endif
	
	dep_wayland_client = dependency('wayland-client', version: '>= 1.22.0', required: wayland_enabled)
else
	# macOS - xkbcommon not needed, use disabler to make dependencies happy
	dep_xkbcommon = disabler()
	dep_wayland_client = disabler()
	dep_pixman = disabler()
endif

if is_linux
  # pixman is only needed for Wayland backend on Linux
  dep_pixman = dependency('pixman-1', version: '>= 0.25.2')
else
  dep_pixman = disabler()
endif

if not is_windows
  dep_libm = cc.find_library('m')
  dep_libdl = cc.find_library('dl')
else
  dep_libm = disabler()
  dep_libdl = disabler()
endif

# Thread dependency (winpthreads on Windows, pthread elsewhere)
dep_threads = dependency('threads')

dep_pango = dependency('pango')
dep_pangocairo = dependency('pangocairo')
dep_fontconfig = dependency('fontconfig')
dep_glib = dependency('glib-2.0', version: '>= 2.36')

# ICU dependency handling
if is_darwin
	# On macOS, ICU (icu4c) is keg-only in Homebrew
	# User must set PKG_CONFIG_PATH or we try to find it
	
	message('Looking for ICU on macOS...')
	message('If this fails, run: export PKG_CONFIG_PATH="/opt/homebrew/opt/icu4c/lib/pkgconfig:$PKG_CONFIG_PATH"')
	message('(or /usr/local/opt/icu4c/lib/pkgconfig on Intel Macs)')
	
	# Try with relaxed version requirement
	dep_icu_i18n = dependency('icu-i18n', version: '>= 60.0', required: false)
	dep_icu_uc = dependency('icu-uc', version: '>= 60.0', required: false)
	
	if not dep_icu_i18n.found() or not dep_icu_uc.found()
		# Try to help by checking common Homebrew locations
		homebrew_prefixes = ['/opt/homebrew', '/usr/local']
		icu_hint = ''
		foreach prefix : homebrew_prefixes
			icu_pc_path = prefix / 'opt/icu4c/lib/pkgconfig'
			if run_command('test', '-d', icu_pc_path, check: false).returncode() == 0
				icu_hint = '\n  Found ICU at: ' + icu_pc_path + '\n  Please run: export PKG_CONFIG_PATH="' + icu_pc_path + ':$PKG_CONFIG_PATH"'
				break
			endif
		endforeach
		
		if icu_hint == ''
			error('ICU not found. Please install with: brew install icu4c')
		else
			error('ICU is installed but pkg-config cannot find it.' + icu_hint)
		endif
	endif
	
	dep_icu = [dep_icu_i18n, dep_icu_uc]
	message('Found ICU: i18n=' + dep_icu_i18n.version() + ', uc=' + dep_icu_uc.version())
else
	# Linux typically has newer ICU in standard locations
	dep_icu = [
	  dependency('icu-i18n', version: '>= 72.0'),
	  dependency('icu-uc', version: '>= 72.0'),
	]
endif

deps_cairo_shared = [
	dependency('cairo'),
	dependency('libpng'),
	dep_libm,
]

# Add pixman only on Linux (needed for Wayland backend)
if is_linux
  deps_cairo_shared += dep_pixman
endif

if dep_pango.found() and dep_pangocairo.found() and dep_fontconfig.found() and dep_glib.found()
	deps_cairo_shared += [ dep_pango, dep_pangocairo, dep_fontconfig, dep_glib ]
	config_h.set('HAVE_PANGO', '1')
endif

# JPEG library detection - try multiple names (OPTIONAL)
dep_libjpeg = dependency('libjpeg', required: false)
if not dep_libjpeg.found()
	# Try alternative pkg-config name (jpeg-turbo on some systems)
	dep_libjpeg = dependency('libjpeg-turbo', required: false)
endif
if not dep_libjpeg.found()
	# Fallback to finding library directly
	dep_libjpeg = cc.find_library('jpeg', required: false)
endif

if dep_libjpeg.found()
	deps_cairo_shared += dep_libjpeg
	config_h.set('HAVE_JPEG', '1')
	message('JPEG support: enabled (translator will be built)')
else
	warning('JPEG library not found - JPEG translator will NOT be built. Install with: brew install jpeg-turbo (macOS) or apt install libjpeg-dev (Linux)')
endif

# WebP library detection (OPTIONAL)
dep_webp = dependency('libwebp', required: false)
if dep_webp.found()
	deps_cairo_shared += dep_webp
	config_h.set('HAVE_WEBP', '1')
	message('WebP support: enabled (translator will be built)')
else
	warning('WebP library not found - WebP translator will NOT be built. Install with: brew install webp (macOS) or apt install libwebp-dev (Linux)')
endif

subdir('src/libs/mapm')
subdir('src/libs/agg')
subdir('src/libs/icon')
subdir('src/system')    # libroot (contains backend subdirs; they honor options internally)
subdir('src/kits')      # libbe / libtracker
subdir('src/tools')     # test framework
subdir('src/bin')       # rc (and friends)

# Only now do we have rc & xres to create the resources for libbe and libtracker
# Note: Resource embedding only works for shared libraries on Linux/macOS
# On Windows with static libs, skip resource embedding for now
if not is_windows
    srcs_libbe_resources = [
        'src/kits/CountryFlags.rdef',
        'src/kits/Icons.rdef',
        'src/kits/LanguageFlags.rdef',
        'src/kits/libbe_version.rdef',
    ]

    libbe_resources = custom_target('libbe resources',
        depends : lib_libbe,
        input : srcs_libbe_resources,
        output : 'libbe.rsrc',
        command : [rc_app, '@INPUT@', '-o', '@OUTPUT@', '-I', '.'],
        build_by_default : true)

    custom_target('libbe plus resources',
        depends : libbe_resources,
        input : lib_libbe,
        output : 'fake-libbe',
        command : [xres_app, '-o', '@INPUT@', libbe_resources],
        build_by_default : true)

    srcs_libtracker_resources = [
        'src/kits/tracker/libtracker.rdef',
        'src/kits/tracker/TrackerIcons.rdef',
    ]

    libtracker_include_deps = custom_target('libtracker resource includes',
        input: [meson.global_source_root() + '/src/kits/tracker/TrackerIcons.h'],
        output: ['res'],
        command: ['cp', '-r', meson.global_source_root() + '/src/kits/tracker/TrackerIcons.h', meson.global_build_root() + '/src/kits/tracker/'],
        install: false,
        build_by_default: true
    )

    libtracker_resources = custom_target('libtracker resources',
        depends : [lib_libtracker, libtracker_include_deps, rc_app],
        depend_files : meson.global_source_root() + '/src/kits/tracker/TrackerIcons.h',
        input : srcs_libtracker_resources,
        output : 'libtracker.rsrc',
        command : [rc_app, '@INPUT@', '-o', '@OUTPUT@', '-I', '.', '-I', meson.global_build_root() + '/src/kits/tracker'],
        build_by_default : true)

    custom_target('libtracker plus resources',
        depends : libtracker_resources,
        input : lib_libtracker,
        output : 'fake-libtracker',
        command : [xres_app, '-o', '@INPUT@', libtracker_resources],
        build_by_default : true)
endif

subdir('src/apps')
subdir('src/add-ons')
if unit_tests_enabled
  subdir('src/tests')
endif
subdir('data')
subdir('headers')


configure_file(input : 'config.h.meson',
    output : 'config.h',
    configuration : config_h)

# Cosmoe relies on its libraries being in /usr/local/lib, so we need to make
# accommodations for e.g. Redhat-based distributions that use /usr/lib64 instead.
install_data(
  'cosmoe.conf',
  install_dir: '/etc/ld.so.conf.d')

meson.add_install_script('sh', '-c', 'ldconfig')
