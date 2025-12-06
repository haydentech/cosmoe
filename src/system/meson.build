subdir('kernel')
subdir('libroot')
subdir('wayland')

# Build backend plugins
subdir('X11')
subdir('wayland/backend')



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
