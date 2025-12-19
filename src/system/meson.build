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
