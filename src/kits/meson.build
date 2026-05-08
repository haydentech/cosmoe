# source_root = [ 'app', 'game', 'interface', 'locale', 'storage', 'support', 'shared', 'translation', 'textencoding',
# 		'../system/kernel', '../system/kernel/util', 'interface/layouter', 'interface/textview_support', 'storage/mime',
# 		'storage/disk_device', 'storage/disk_device/jobs', 'storage/sniffer',
# 		'../system/libroot/os', '../system/libroot/os/locks',
# 		'../servers/wayland/clients', '../servers/wayland/shared', '../servers/wayland/protocol'
# ]

subdir('app')
subdir('game')
subdir('interface')
subdir('locale')
subdir('media')
subdir('storage')
subdir('support')
subdir('shared')
subdir('translation')
subdir('textencoding')


# Use wayland_enabled from parent (already accounts for platform)
# Don't re-query the option here as it doesn't account for is_darwin

deps_libbe = [
    dep_icu,

    dep_libmapm,
	dep_libroot,
	dep_libicon,
	dep_libagg,
	dep_threads,
	dep_zlib,
]

# On macOS and Windows, iconv is needed for libbe but not passed to dependents
# (it's a pure library dependency with no headers needed by users)
deps_libbe_private = []
if host_machine.system() == 'darwin'
    iconv_dep = dependency('iconv', required: false)
    if not iconv_dep.found()
        # On macOS, iconv is part of the system, try finding it as a library
        iconv_dep = cc.find_library('iconv', required: true)
    endif
    deps_libbe_private += [iconv_dep]
elif is_windows
    # On Windows (MSYS2), iconv is provided as a library
    iconv_dep = cc.find_library('iconv', required: true)
    deps_libbe_private += [iconv_dep]
endif

# Add optional Wayland dependencies to libbe only if Wayland backend is enabled
if wayland_enabled
    deps_libbe += [
        dep_xkbcommon,
        dependency('wayland-cursor', required: true),
        dep_wayland_client,
    ]
endif

# Haiku-compatible localestub: linked into each image that depends on libbe.
lib_localestub = static_library(
    'localestub',
    srcs_localestub,
    include_directories: [common_inc, tracker_inc],
    dependencies: deps_libbe,
    pic: true,
    install: true,
)

# Build bootstrap libbe (without resources) - only used by rc
# Real libbe with resources will be built after rc exists (see main meson.build)
lib_libbe_bootstrap = shared_library(
    'be-bootstrap',
    [srcs_app,
        srcs_media,
     srcs_interface,
     srcs_storage,
     srcs_support,
     srcs_shared,
     srcs_translation,
     srcs_locale,
     srcs_textencoding,
     is_windows ? 'libbe_dllmain.cpp' : [],
	],
    include_directories: [common_inc, tracker_inc],
    dependencies: [deps_libbe, is_windows ? [] : deps_cairo_shared, deps_libbe_private],
    link_with: [lib_localestub],
    link_whole: [lib_libroot],
    c_args: is_windows ? ['-DBUILDING_LIBBE'] : [],
    cpp_args: is_windows ? ['-DBUILDING_LIBBE'] : [],
    link_args: is_windows ? ['-Wl,--export-all-symbols', '-Wl,--enable-auto-import', '-Wl,--enable-runtime-pseudo-reloc'] : [],
    install: false,  # Don't install bootstrap, only real libbe
)

dep_libbe_bootstrap = declare_dependency(
    link_with: [lib_libbe_bootstrap, lib_localestub] + (is_darwin ? lib_macmain_export : []),
    dependencies: deps_libbe,
)

# For now, dep_libbe points to bootstrap (will be overridden in main meson.build after rc is built)
dep_libbe = dep_libbe_bootstrap
lib_libbe = lib_libbe_bootstrap

# Don't build tracker here - it will be built in main meson.build after real libbe

