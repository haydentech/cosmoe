project('cosmoe',
	['c', 'cpp'],
	version: '0.9.0',
	license: 'MIT',
	meson_version: '>=1.1.0',
	default_options: ['cpp_std=c++17']
)

version_cosmoe = meson.project_version()

dir_prefix = get_option('prefix')
dir_bin = dir_prefix / get_option('bindir')
dir_data = dir_prefix / get_option('datadir')
dir_lib = dir_prefix / get_option('libdir')

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

	if (wayland_enabled and not x11_enabled)
		message('Linux detected: Building Wayland backend only')
	elif (x11_enabled and not wayland_enabled)
		message('Linux detected: Building X11 backend only')
	else
		message('Linux detected: Building X11 and Wayland backends')
	endif
endif

unit_tests_enabled = get_option('enable_unit_test_compile')

# Initialize windres to disabler (will be replaced if on Windows)
windres = disabler()

# When cross-compiling to Windows, we need a native 'rc' tool to compile resources
# Check for it early to give a clear error message
if meson.is_cross_build() and is_windows
	rc_native = find_program('rc', native: true, required: false)
	if not rc_native.found()
		error('Cross-compiling to Windows requires a native "rc" tool to be installed.\n' +
		      'Please install cosmoe on the build host first, or ensure "rc" is in your PATH.')
	endif
	message('Found native rc tool for cross-compilation: ' + rc_native.full_path())
	
	# Also need windres for compiling Windows .rc files to .o format
	windres = find_program('windres', required: true)
	message('Found windres tool: ' + windres.full_path())
endif

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
	
	# Prevent duplicate -lc++ warnings: Objective-C++ automatically links libc++
	# Override the default link argument to avoid duplication
	add_project_link_arguments(['-Wl,-no_warn_duplicate_libraries'], language: ['c', 'cpp', 'objc', 'objcpp'])
	
	# Add rpath to find libraries in the installation prefix
	prefix = get_option('prefix')
	libdir = get_option('libdir')
	if not libdir.startswith('/')
		# libdir is relative to prefix
		full_libdir = prefix / libdir
	else
		# libdir is absolute
		full_libdir = libdir
	endif
	add_project_link_arguments(['-Wl,-rpath,' + full_libdir], language: ['c', 'cpp', 'objc', 'objcpp'])
endif

# On Linux, also add rpath to find libraries in the installation directory
if is_linux
	prefix = get_option('prefix')
	libdir = get_option('libdir')
	if not libdir.startswith('/')
		# libdir is relative to prefix
		full_libdir = prefix / libdir
	else
		# libdir is absolute
		full_libdir = libdir
	endif
	add_project_link_arguments(['-Wl,-rpath,' + full_libdir], language: ['c', 'cpp'])
endif

# On Windows/MSYS2, ensure _WIN32 is defined for MinGW headers
if is_windows
	add_project_arguments(['-D_WIN32', '-DWIN32', '-DUNICODE', '-D_UNICODE'], language: 'c')
	add_project_arguments(['-D_WIN32', '-DWIN32', '-DUNICODE', '-D_UNICODE'], language: 'cpp')
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
	'headers/os/be_apps/Tracker',
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
	'headers/os/media',
	'headers/os/package',
	'headers/os/add-ons/file_system',
	'headers/os/mail',
	'headers/os/net',
	'headers/private/app',
	'headers/private/shared',
	'headers/private/interface',
	'headers/private/locale',
	'headers/private/mount',
	'headers/private/storage',
	'headers/private/system',
	'headers/private/net',
	'headers/private/tracker',
	'headers/os/add-ons/graphics',
	'src/kits/interface/textview_support',
	'src/kits/interface/layouter',
	'src/kits/storage/disk_device',
	'src/kits/storage/disk_device/jobs',
	'src/kits/storage',		# For sniffer support
	'headers/libs/wayland/stubs',
	'headers/private',
	'headers/private/input',
	'headers/private/graphics',
	'headers/private/kernel',
	'headers/private/kernel/util',
	'headers/private/libroot',
	'headers/private/media',
	'headers/private/netservices2',
	'headers/private/support',
	'headers/private/print',
	'headers/private/textencoding',
	'headers/posix'
]
if wayland_enabled
	public_inc_entries += [
		'headers/libs/wayland',
		'headers/libs/wayland/shared',
		'headers/libs/wayland/protocol',
	]
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
else
	warning('JPEG library not found - JPEG translator will NOT be built. Install with: brew install jpeg-turbo (macOS) or apt install libjpeg-dev (Linux)')
endif

# WebP library detection (OPTIONAL)
dep_webp = dependency('libwebp', required: false)
if dep_webp.found()
	deps_cairo_shared += dep_webp
	config_h.set('HAVE_WEBP', '1')
else
	warning('WebP library not found - WebP translator will NOT be built. Install with: brew install webp (macOS) or apt install libwebp-dev (Linux)')
endif

# Zlib library detection (REQUIRED for compression support)
dep_zlib = dependency('zlib', required: true)

subdir('src/libs/mapm')
subdir('src/libs/agg')
subdir('src/libs/icon')
subdir('src/system')    # libroot + kernel only (backends moved to src/backends)
subdir('src/kits')      # libbe-bootstrap / libtracker
subdir('src/bin')       # rc (uses libbe-bootstrap)

# Build real libbe with embedded resources (after rc is available)
# Compile resource files (same for all platforms)
libbe_rsrc = custom_target('libbe_rsrc',
        input: [
            'src/kits/libbe_version.rdef',
            'src/kits/CountryFlags.rdef',
            'src/kits/Icons.rdef',
            'src/kits/LanguageFlags.rdef',
        ],
        output: 'libbe.rsrc',
        command: [rc_app, '-o', '@OUTPUT@', '@INPUT@'],
    )
        
    # Platform-specific resource object creation and library linking
    if is_darwin
        libbe_rsrc_o = custom_target('libbe_rsrc_o',
            input: libbe_rsrc,
            output: 'libbe_resources.o',
            command: ['sh', '-c', 'echo "int dummy;" > /tmp/empty_$$.c && cc -c /tmp/empty_$$.c -o /tmp/empty_$$.o && ld -r -o @OUTPUT@ -sectcreate __TEXT __rsrc @INPUT@ /tmp/empty_$$.o && rm /tmp/empty_$$.c /tmp/empty_$$.o'],
        )
        
        # Build real libbe with resources embedded
        lib_libbe = shared_library(
            'be',
            objects: lib_libbe_bootstrap.extract_all_objects(recursive: false),
            include_directories: [common_inc, tracker_inc],
            dependencies: [deps_libbe, deps_cairo_shared, deps_libbe_private],
			link_with: [lib_localestub],
            link_whole: [lib_libroot],
            link_args: ['-Wl,-sectcreate,__TEXT,__rsrc,' + meson.current_build_dir() / 'libbe.rsrc'],
            link_depends: [libbe_rsrc],
            install_dir: dir_lib,
            install: true,
        )
        
        dep_libbe = declare_dependency(
			link_with: [lib_libbe, lib_localestub, lib_macmain_export],
            dependencies: deps_libbe,
        )
    elif is_linux
        # On Linux, embed the full .rsrc file (with RS magic header)
        # Use objcopy to rename symbols to standard names expected by ResourceFile.cpp
        libbe_resources_obj = custom_target('libbe_resources_obj',
            input: libbe_rsrc,
            output: 'libbe_rsrc.o',
			command: [
				'sh', '-c',
				'OUTPUT_ABS=$(cd $(dirname "$2") && pwd)/$(basename "$2") &&' +
				# Create object with embedded binary, add .note.GNU-stack, and rename symbols
				'ld -r -b binary "$1" -o "$2" && ' +
				# Rename .data section to .cosmoe_rsrc so it doesn't merge with executable's .data
				# which can prevent us from finding it at runtime
				'objcopy --rename-section .data=.cosmoe_rsrc "$OUTPUT_ABS" && ' +
				'objcopy ' +
				'--add-section .note.GNU-stack=/dev/null ' +
				'--set-section-flags .note.GNU-stack=noload,readonly "$2"',
				'--', '@INPUT@', '@OUTPUT@'
			],
        )
        
        # Build real libbe with resources embedded
        lib_libbe = shared_library(
            'be',
            objects: [lib_libbe_bootstrap.extract_all_objects(recursive: false), libbe_resources_obj],
            include_directories: [common_inc, tracker_inc],
            dependencies: [deps_libbe, deps_cairo_shared, deps_libbe_private],
			link_with: [lib_localestub],
            link_whole: [lib_libroot],
            install_dir: dir_lib,
            install: true,
        )
        
        dep_libbe = declare_dependency(
			link_with: [lib_libbe, lib_localestub],
            dependencies: deps_libbe,
        )
    elif is_windows
        # Windows: Use objcopy to embed resources in PE/COFF format
        # Use full .rsrc file (with RS magic header) so ResourceFile can find it
        libbe_resources_obj = custom_target('libbe_resources_obj',
            input: libbe_rsrc,
            output: 'libbe_rsrc.o',
            command: [
                'sh', '-c',
                'objcopy -I binary -O pe-x86-64 -B i386:x86-64 ' +
                '--rename-section .data=.cosmoe_rsrc,alloc,load,readonly,data,contents ' +
                '"$1" "$2"',
                '--', '@INPUT@', '@OUTPUT@'
            ],
        )
        
        # Build real libbe.dll with resources embedded
        lib_libbe = shared_library(
            'be',
            objects: [lib_libbe_bootstrap.extract_all_objects(recursive: false), libbe_resources_obj],
            include_directories: [common_inc, tracker_inc],
            dependencies: [deps_libbe, deps_libbe_private],
			link_with: [lib_localestub],
            link_whole: [lib_libroot],
            c_args: ['-DBUILDING_LIBBE'],
            cpp_args: ['-DBUILDING_LIBBE'],
            link_args: ['-Wl,--export-all-symbols', '-Wl,--enable-auto-import', '-Wl,--enable-runtime-pseudo-reloc'],
            install_dir: get_option('libdir'),
            install: true,
        )
        
        dep_libbe = declare_dependency(
			link_with: [lib_libbe, lib_localestub],
            dependencies: deps_libbe,
        )
    endif

	# Build Game Kit as standalone library (libgame), dependent on real libbe
	if get_option('enable_libgame')
		lib_libgame = shared_library(
			'game',
			srcs_game,
			include_directories: [common_inc, tracker_inc],
			dependencies: [dep_libbe],
			install_dir: dir_lib,
			install: true,
		)

		dep_libgame = declare_dependency(
			link_with: [lib_libgame],
			dependencies: [dep_libbe],
		)
	endif

# Now build everything else with the real libbe

# Build libtracker with real libbe (not bootstrap)
subdir('src/kits/tracker')

# Build network libraries with real libbe
subdir('src/kits/network')

# Build bin tools with real libbe (they were skipped in src/bin to avoid bootstrap)

# Build the final rc with real libbe (for installation)
# For native non-cross builds, we built rc-bootstrap earlier that links to libbe-bootstrap
# Now that libbe is available, build the real rc that will be installed
if not meson.is_cross_build()
    # Re-use the flex/bison generators from src/bin/rc
    flex = find_program('flex', required: true)
    bison = find_program('bison', required: true)
    
    lgen = generator(flex,
        output : '@PLAINNAME@.yy.cpp',
        arguments : ['-o', '@OUTPUT@', '@INPUT@'])
    
    lfiles_final = lgen.process('src/bin/rc/lexer.l')
    
    bgen = generator(bison,
        output : ['@BASENAME@.tab.cpp', '@BASENAME@.hpp'],
        arguments : ['@INPUT@', '--defines=@OUTPUT1@', '--output=@OUTPUT0@'])
    
    bfiles_final = bgen.process('src/bin/rc/parser.y')
    
    # Build final rc linked to real libbe
    rc_final = executable(
        'rc',
        'src/bin/rc/compile.cpp',
        'src/bin/rc/decompile.cpp',
        'src/bin/rc/rc.cpp',
        'src/bin/rc/rdef.cpp',
        lfiles_final,
        bfiles_final,
        dependencies: dep_libbe,
        include_directories: [common_inc, include_directories('src/bin/rc')],
        install: true,
        install_dir: get_option('bindir'),
    )
    message('Native build: Building final rc with real libbe (for installation)')
endif

xres_app = executable(
    'xres',
    'src/bin/xres.cpp',
    dependencies: dep_libbe,
    include_directories: common_inc,
    install: true,
    install_dir: get_option('bindir'),
)

listres_app = executable(
    'listres',
    'src/bin/listres.cpp',
    dependencies: dep_libbe,
    include_directories: common_inc,
    install: true,
    install_dir: get_option('bindir'),
)

catattr_app = executable(
    'catattr',
    'src/bin/catattr.cpp',
    dependencies: dep_libbe,
    include_directories: common_inc,
    install: true,
    install_dir: get_option('bindir'),
)

copyattr_app = executable(
    'copyattr',
    'src/bin/copyattr.cpp',
    dependencies: dep_libbe,
    include_directories: common_inc,
    install: true,
    install_dir: get_option('bindir'),
)

listattr_app = executable(
    'listattr',
    'src/bin/listattr.cpp',
    dependencies: dep_libbe,
    include_directories: common_inc,
    install: true,
    install_dir: get_option('bindir'),
)

addattr_app = executable(
	'addattr',
	['src/bin/addattr/addAttr.cpp', 'src/bin/addattr/main.cpp'],
	dependencies: dep_libbe,
	include_directories: common_inc,
	install: true,
	install_dir: get_option('bindir'),
)

message_app = executable(
    'message',
    'src/bin/message.cpp',
    dependencies: dep_libbe,
    include_directories: common_inc,
    install: true,
    install_dir: get_option('bindir'),
)

setcontrollook_app = executable(
    'setcontrollook',
    'src/bin/setcontrollook.cpp',
    dependencies: dep_libbe,
    include_directories: common_inc,
    install: true,
    install_dir: get_option('bindir'),
)

mime_db_builder_app = executable(
	'mime_db_builder',
	'src/bin/mime_db_builder.cpp',
	dependencies: dep_libbe,
	include_directories: common_inc,
	install: false,
)

mimeset_app = executable(
	'mimeset',
	[
		'src/bin/mimeset.cpp',
		'src/kits/storage/mime/AppMetaMimeCreator.cpp',
		'src/kits/storage/mime/MimeEntryProcessor.cpp',
		'src/kits/storage/mime/MimeInfoUpdater.cpp',
	],
	dependencies: dep_libbe,
	include_directories: common_inc,
	install: true,
	install_dir: get_option('bindir'),
)


finddir_app = executable(
    'finddir',
    'src/bin/finddir.c',
    dependencies: dep_libbe,
    include_directories: common_inc,
    install: true,
    install_dir: get_option('bindir'),
)

findpaths_app = executable(
    'findpaths',
    'src/bin/findpaths.cpp',
    dependencies: dep_libbe,
    include_directories: common_inc,
    install: true,
    install_dir: get_option('bindir'),
)

roster_app = executable(
    'roster',
    'src/bin/roster.cpp',
    dependencies: dep_libbe,
    include_directories: common_inc,
    install: true,
    install_dir: get_option('bindir'),
)

mkdepend_app = executable(
    'mkdepend',
    [
        'src/bin/mkdepend/args.c',
        'src/bin/mkdepend/getargs.c',
        'src/bin/mkdepend/main.c',
        'src/bin/mkdepend/nodes.c',
        'src/bin/mkdepend/reader.c',
        'src/bin/mkdepend/util.c',
    ],
    include_directories: [common_inc, include_directories('src/bin/mkdepend')],
    install: true,
    install_dir: get_option('bindir'),
)

collectcatkeys_app = executable(
	'collectcatkeys',
	[
		'src/tools/locale/collectcatkeys.cpp',
		'src/tools/locale/PlainTextCatalog.cpp',
		'src/tools/locale/Catalog.cpp',
		'src/tools/locale/CatalogData.cpp',
		'src/kits/locale/HashMapCatalog.cpp',
		'src/kits/tracker/RegExp.cpp',
	],
	dependencies: dep_libbe,
	include_directories: [include_directories('src/kits/tracker'), common_inc],
	cpp_args: [
		'-D_HAIKU_BUILD_DONT_REMAP_FD_FUNCTIONS',
		'-DB_AVOID_TRANSLATION_MACROS',
		'-DB_TRANSLATE_NOCOLLECT(x)=x',
		'-DB_TRANSLATE_MARK(x)=x',
		'-DDefaultCatalog=ToolsDefaultCatalog',
	],
	install: false,
)

linkcatkeys_app = executable(
	'linkcatkeys',
	[
		'src/tools/locale/linkcatkeys.cpp',
		'src/tools/locale/PlainTextCatalog.cpp',
		'src/tools/locale/DefaultCatalog.cpp',
		'src/tools/locale/Catalog.cpp',
		'src/tools/locale/CatalogData.cpp',
		'src/kits/locale/HashMapCatalog.cpp',
	],
	dependencies: dep_libbe,
	include_directories: common_inc,
	cpp_args: [
		'-D_HAIKU_BUILD_DONT_REMAP_FD_FUNCTIONS',
		'-DB_AVOID_TRANSLATION_MACROS',
		'-DB_TRANSLATE_NOCOLLECT(x)=x',
		'-DB_TRANSLATE_MARK(x)=x',
		'-DDefaultCatalog=ToolsDefaultCatalog',
	],
	install: false,
)

dumpcatalog_app = executable(
	'dumpcatalog',
	'src/bin/locale/dumpcatalog.cpp',
	dependencies: dep_libbe,
	include_directories: common_inc,
	install: true,
	install_dir: get_option('bindir'),
)

# getarch - Shell script for makefile-engine compatibility
install_data(
	'src/bin/getarch',
	install_dir: get_option('bindir'),
	install_mode: 'rwxr-xr-x',  # Make it executable
)

# vicn2icns - Convert VICN resources to macOS .icns format (macOS only)
if is_darwin
    dep_libpng = dependency('libpng')
    vicn2icns_prog = executable(
        'vicn2icns',
        'src/bin/vicn2icns.cpp',
        dependencies: [dep_libbe, dep_libpng],
        include_directories: common_inc,
        install: true,
        install_dir: get_option('bindir'),
    )
else
    # On non-Darwin platforms, set to disabler so apps can conditionally use it
    vicn2icns_prog = disabler()
endif

# vicn2png - Convert VICN resources to PNG icons for X11/Wayland (Linux only)
if is_linux
    dep_libpng = dependency('libpng')
    vicn2png_prog = executable(
        'vicn2png',
        'src/bin/vicn2png.cpp',
        dependencies: [dep_libbe, dep_libpng],
        include_directories: common_inc,
        install: true,
        install_dir: get_option('bindir'),
    )
else
    # On non-Linux platforms, set to disabler so apps can conditionally use it
    vicn2png_prog = disabler()
endif

# vicn2ico - Convert VICN resources to .ico format (Windows only)
# Always build as native executable since it runs during the build process
if is_windows
    dep_libpng_native = dependency('libpng', native: true)
    vicn2ico_prog = executable(
        'vicn2ico',
        'src/bin/vicn2ico.cpp',
        dependencies: [dep_libpng_native],
        link_args: ['-lbe'],
        include_directories: common_inc,
		cpp_args: c_args + cpp_args,
        native: true,
        install: false,
    )
else
    # On non-Windows platforms, set to disabler so apps can conditionally use it
    vicn2ico_prog = disabler()
endif

subdir('src/backends')  # backend plugins (X11, Wayland, Windows, Cocoa) — built after libbe

subdir('src/tools')     # test framework (uses real libbe)

subdir('src/apps')
subdir('src/preferences')
subdir('src/add-ons')
if unit_tests_enabled
  subdir('src/tests')
endif
subdir('data')
subdir('headers')
subdir('src/servers/app/fonts')  # Font installation (macOS only)

configure_file(input : 'config.h.meson',
    output : 'config.h',
    configuration : config_h)

# Clean up stale libbe.so from /usr/local/lib if installing to a different directory
# This prevents the dynamic linker from finding an outdated library
if dir_lib != '/usr/local/lib'
    meson.add_install_script('sh', '-c', 
        'if [ -f /usr/local/lib/libbe.so ]; then ' +
        'echo "Removing stale libbe.so from /usr/local/lib (now installing to ' + dir_lib + ')"; ' +
        'rm -f /usr/local/lib/libbe.so /usr/local/lib/libbe.so.*; ' +
        'fi')
endif


