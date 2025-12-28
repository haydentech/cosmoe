subdir('kernel')
subdir('libroot')

# Use backend enabled flags from parent meson.build
# These already account for platform (is_darwin, is_linux)
# Don't re-query options here as they don't account for platform detection

# Build backend plugins only if enabled
if wayland_enabled
	subdir('wayland')
	subdir('wayland/backend')
endif

if x11_enabled
    subdir('X11')
endif

if cocoa_enabled
    subdir('cocoa')
endif

if windows_enabled
    subdir('windows')
endif



deps_libroot = [
	dep_pangocairo,
	dep_pango,
    dep_webp,
    dependency('cairo'),
	dependency('libpng'),
]

# Add Linux-specific dependencies only when Wayland is enabled
if wayland_enabled
    deps_libroot += [dep_xkbcommon, dep_pixman]
endif

# Add Windows-specific libraries for process/module enumeration
if host_machine.system() == 'windows'
    # On Windows, we need psapi.lib for EnumProcessModules and GetModuleInformation
    cc = meson.get_compiler('c')
    deps_libroot += [cc.find_library('psapi', required: true)]
endif

lib_libroot = static_library(
	'root',
	[srcs_root, srcs_kernel],
	include_directories: common_inc,
	dependencies: deps_libroot,
	install: false,
)

dep_libroot = declare_dependency(
	link_with: lib_libroot,
	dependencies: deps_libroot,
)
