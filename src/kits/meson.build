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

if is_linux
    deps_libbe += [dep_xkbcommon]
endif

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
        dependency('wayland-cursor', required: true),
        dep_wayland_client,
    ]
endif

# Everything that goes into libbe.  The real library is linked in
# meson.build once the resources it embeds have been compiled by rc.
srcs_libbe = [
    srcs_app,
    srcs_media,
    srcs_interface,
    srcs_storage,
    srcs_support,
    srcs_shared,
    srcs_translation,
    srcs_locale,
    srcs_textencoding,
]

# Haiku-compatible localestub: linked into each image that depends on libbe.
lib_localestub = static_library(
    'localestub',
    srcs_localestub,
    include_directories: [common_inc, tracker_inc],
    dependencies: deps_libbe,
    pic: true,
    install: true,
)

# ---- libbe for the build machine ------------------------------------------
#
# rc, vicn2ico and the install-time helpers (mime_db_builder, linkcatkeys) use
# the Be API, so they need libbe for the machine the build runs on.  When
# cross-compiling, that is not the libbe being built for the target, and it
# cannot be borrowed from a previous native build either: the target libbe
# needs the resources rc compiles, and rc needs libbe.  Compiling the kit
# sources once for the build machine is what breaks that cycle, so a cross
# build needs no native build or install of its own beforehand.
#
# Native builds have only one machine, so they keep the cheaper arrangement:
# compile the kits once into a shared bootstrap library, link the tools
# against it, and let the installed libbe reuse its objects.
if meson.is_cross_build()
    deps_libbe_build = [
        dependency('icu-i18n', native: true),
        dependency('icu-uc', native: true),
        dependency('threads', native: true),
        dependency('zlib', native: true),
        dependency('cairo', native: true),
        dependency('libpng', native: true),
        dependency('pango', native: true),
        dependency('pangocairo', native: true),
        dependency('fontconfig', native: true),
        dependency('glib-2.0', native: true),
    ]
    if build_machine.system() == 'linux'
        deps_libbe_build += [
            dependency('pixman-1', native: true),
            dependency('xkbcommon', native: true),
        ]
    endif

    # Project arguments only reach host-machine targets in a cross build, so
    # the build machine's platform and its feature header have to be described
    # here explicitly.  -include is needed because <config.h> would otherwise
    # resolve to the target's header in the build directory.
    libbe_build_args = []
    if build_machine.system() == 'linux'
        libbe_build_args += '-DHAIKU_HOST_PLATFORM_LINUX'
    elif build_machine.system() == 'darwin'
        libbe_build_args += '-DHAIKU_HOST_PLATFORM_DARWIN'
    endif
    libbe_build_args += [
        '-include',
        meson.project_build_root() / 'config_build.h',
    ]

    # libroot, mapm, agg and icon are folded in rather than linked: a native
    # target cannot link the target-machine build of them.
    lib_libbe_tool = static_library(
        'be-build',
        [
            srcs_root,
            srcs_kernel_build,
            srcs_localestub,
            srcs_app,
            srcs_media,
            srcs_interface,
            srcs_storage,
            srcs_support_build,
            srcs_shared,
            srcs_translation,
            srcs_locale,
            srcs_textencoding,
        ],
        include_directories: [common_inc, tracker_inc, icon_inc, agg_inc],
        dependencies: deps_libbe_build,
        link_with: [lib_libagg_build, lib_libmapm_build, lib_libicon_build],
        c_args: libbe_build_args,
        cpp_args: libbe_build_args,
        pic: true,
        native: true,
        install: false,
    )

    # link_whole, because libbe's own initialization (BMessage's cache, for one)
    # lives in a constructor-only object that a normal archive link drops.  It
    # also means every tool sees the locale kit, so the copies of the catalog
    # classes in src/tools/locale are not compiled into the tools below.
    dep_libbe_tool = declare_dependency(
        link_whole: lib_libbe_tool,
        dependencies: deps_libbe_build,
    )
else
    lib_libbe_tool = shared_library(
        'be-bootstrap',
        [srcs_libbe, is_windows ? 'libbe_dllmain.cpp' : []],
        include_directories: [common_inc, tracker_inc],
        dependencies: [deps_libbe, is_windows ? [] : deps_cairo_shared, deps_libbe_private],
        link_with: [lib_localestub],
        link_whole: [lib_libroot],
        c_args: is_windows ? ['-DBUILDING_LIBBE'] : [],
        cpp_args: is_windows ? ['-DBUILDING_LIBBE'] : [],
        link_args: is_windows ? ['-Wl,--export-all-symbols', '-Wl,--enable-auto-import', '-Wl,--enable-runtime-pseudo-reloc'] : [],
        install: false,  # Don't install the bootstrap, only the real libbe
    )

    dep_libbe_tool = declare_dependency(
        link_with: [lib_libbe_tool, lib_localestub] + (is_darwin ? lib_macmain_export : []),
        dependencies: deps_libbe,
    )
endif

# Don't build tracker here - it will be built in main meson.build after real libbe

