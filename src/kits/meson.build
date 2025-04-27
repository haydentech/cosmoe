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


deps_libbe = [
	dependency('wayland-cursor'),
	dep_wayland_client,
	dep_xkbcommon,

	dep_icu,

	dep_libmapm,
	dep_libroot,
	dep_linprog,
	dep_libicon,
	dep_libagg,
]


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
    include_directories: common_inc,
    dependencies: [deps_libbe, deps_cairo_shared],
    link_whole: [lib_libmapm, lib_libagg, lib_linprog, lib_libicon, lib_libroot],
    install: true,
)

dep_libbe = declare_dependency(
	link_with: lib_libbe,
	dependencies: deps_libbe,
)


srcs_libbe_resources = [
    'CountryFlags.rdef',
    'Icons.rdef',
    'LanguageFlags.rdef',
    'libbe_version.rdef',
]

libbe_resources = custom_target('libbe resources',
    depends : lib_libbe,
    input : srcs_libbe_resources,
    output : 'libbe.rsrc',
    command : ['rc', '@INPUT@', '-o', '@OUTPUT@', '-I', '.'],
    build_by_default : true)

custom_target('libbe plus resources',
    depends : libbe_resources,
    input : lib_libbe,
    output : 'fake',
    command : ['xres', '-o', '@INPUT@', libbe_resources],
    build_by_default : true)