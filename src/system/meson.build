subdir('kernel')
subdir('libroot')

# Conditionally build the Wayland, X11, and Cocoa subprojects
wayland_enabled = get_option('enable_wayland_backend')
x11_enabled = get_option('enable_x11_backend')
cocoa_enabled = get_option('enable_cocoa_backend')

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
	dep_xkbcommon,
	dep_pangocairo,
	dep_pango,
    dep_webp,
    dep_pixman,
    dependency('cairo'),
	dependency('libpng'),
]

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
