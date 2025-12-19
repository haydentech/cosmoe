# source_root = [ 'app', 'game', 'interface', 'locale', 'storage', 'support', 'shared', 'translation', 'textencoding',
# 		'../system/kernel', '../system/kernel/util', 'interface/layouter', 'interface/textview_support', 'storage/mime',
# 		'storage/disk_device', 'storage/disk_device/jobs', 'storage/sniffer',
# 		'../system/libroot/os', '../system/libroot/os/locks',
# 		'../servers/wayland/clients', '../servers/wayland/shared', '../servers/wayland/protocol'
# ]

subdir('app')
subdir('interface')
subdir('locale')
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
	# dep_linprog,  # Unused library, disabled
	dep_libicon,
	dep_libagg,
]

# On macOS, add iconv for text encoding conversion
if host_machine.system() == 'darwin'
    iconv_dep = dependency('iconv', required: false)
    if not iconv_dep.found()
        # On macOS, iconv is part of the system, try finding it as a library
        iconv_dep = cc.find_library('iconv', required: true)
    endif
    deps_libbe += [iconv_dep]
endif

# Add optional Wayland dependencies to libbe only if Wayland backend is enabled
if wayland_enabled
    deps_libbe += [
        dep_xkbcommon,
        dependency('wayland-cursor', required: true),
        dep_wayland_client,
    ]
endif


lib_libbe = shared_library(
    'be',
    [srcs_app,
     srcs_interface,
     srcs_storage,
     srcs_support,
     srcs_shared,
     srcs_translation,
     srcs_locale,
     srcs_textencoding],
    include_directories: [common_inc, tracker_inc],
    install_dir: '/usr/local/lib',
    dependencies: [deps_libbe, deps_cairo_shared],
    link_whole: [lib_libroot],  # lib_linprog removed (unused)
    install: true,
)

dep_libbe = declare_dependency(
	link_with: lib_libbe,
	dependencies: deps_libbe,
)

subdir('tracker')

