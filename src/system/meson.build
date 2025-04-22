subdir('kernel')
# subdir('kernel/util')
subdir('libroot')
# subdir('libroot/os/locks')
subdir('wayland')
# subdir('wayland/shared')
# subdir('wayland/protocol')



deps_libroot = [
	dep_wayland_client,
	dep_xkbcommon,
	dep_pangocairo,
	dep_pango,
    dep_webp,
    dep_pixman,
    dependency('cairo'),
	dependency('libpng'),
	dependency('wayland-cursor')
]

lib_libroot = static_library(
	'root',
	[srcs_root, srcs_kernel, srcs_wayland],
	include_directories: common_inc,
	dependencies: deps_libroot,
	install: false,
)

dep_libroot = declare_dependency(
	link_with: lib_libroot,
	dependencies: deps_libroot,
)
