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
	dep_threads,
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
    pic: true,
)

# On macOS, compile macmain.mm separately and export it as a separate library
# Don't include it in libroot to avoid Mach-O corruption when linked whole into libbe.dylib
libroot_extra_link = []
libroot_extra_deps = []
if host_machine.system() == 'darwin'
    lib_macmain = static_library(
        'macmain',
        srcs_kernel_macos,
        # Note: No common_inc or dependencies - macmain.mm only uses system frameworks
        install: false,
    )
    libroot_extra_link = [lib_macmain]
    # Add Cocoa framework as dependency
    libroot_extra_deps = [dependency('appleframeworks', modules: ['Foundation', 'Cocoa'])]
endif

# Export lib_macmain for direct linking by applications (avoids Mach-O corruption in libbe)
if host_machine.system() == 'darwin'
    lib_macmain_export = lib_macmain
else
    lib_macmain_export = []
endif

dep_libroot = declare_dependency(
	link_with: [lib_libroot],
	dependencies: deps_libroot + libroot_extra_deps,
)
