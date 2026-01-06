srcs_libtracker = [
	'Bitmaps.cpp',
	'FilePanelFileColumn.cpp',
	'FilePanelPoseView.cpp',
	'FilePanel.cpp',
	'FilePanelPriv.cpp',
	'FSUtils.cpp',
	'ContainerWindow.cpp',
	'Model.cpp',
	'Navigator.cpp',
	'Pose.cpp',
	'PoseList.cpp',
	'PoseView.cpp',
	'RegExp.cpp',
	'Utilities.cpp',
	'TrackerInit.cpp'
]

deps_libtracker = [
	dep_libbe,
]

# Compile resource files
libtracker_rsrc = custom_target('libtracker_rsrc',
	input: [
		'libtracker.rdef',
		'TrackerIcons.rdef',
	],
	output: 'libtracker.rsrc',
	command: [rc_app, '-o', '@OUTPUT@', '@INPUT@', '-I', meson.current_source_dir()],
)

# Platform-specific resource object creation
if is_darwin
	libtracker_rsrc_stripped = custom_target('libtracker_rsrc_stripped',
		input: libtracker_rsrc,
		output: 'libtracker.rsrc.stripped',
		command: ['tail', '-c', '+5', '@INPUT@'],
		capture: true,
	)
	
	lib_libtracker = shared_library(
		'tracker',
		srcs_libtracker,
		cpp_args: ['-D_SUPPORTS_RESOURCES=1'],
		include_directories: [common_inc, tracker_inc],
		install_dir: '/usr/local/lib',
		dependencies: deps_libtracker,
		link_args: ['-Wl,-sectcreate,__TEXT,__rsrc,' + meson.current_build_dir() / 'libtracker.rsrc.stripped'],
		link_depends: [libtracker_rsrc_stripped],
		install: true,
	)
elif is_linux
	libtracker_rsrc_stripped = custom_target('libtracker_rsrc_stripped',
		input: libtracker_rsrc,
		output: 'libtracker.rsrc.stripped',
		command: ['dd', 'if=@INPUT@', 'of=@OUTPUT@', 'bs=4', 'skip=1', 'status=none'],
	)
	
	libtracker_resources_obj = custom_target('libtracker_resources_obj',
		input: libtracker_rsrc_stripped,
		output: 'libtracker_rsrc.o',
		command: ['ld', '-r', '-b', 'binary', '@INPUT@', '-o', '@OUTPUT@'],
	)
	
	lib_libtracker = shared_library(
		'tracker',
		srcs_libtracker,
		objects: [libtracker_resources_obj],
		cpp_args: ['-D_SUPPORTS_RESOURCES=1'],
		include_directories: [common_inc, tracker_inc],
		install_dir: '/usr/local/lib',
		dependencies: deps_libtracker,
		install: true,
	)
else
	# Windows or other platforms - no resource embedding yet
	lib_libtracker = shared_library(
		'tracker',
		srcs_libtracker,
		cpp_args: ['-D_SUPPORTS_RESOURCES=1'],
		include_directories: [common_inc, tracker_inc],
		install_dir: '/usr/local/lib',
		dependencies: deps_libtracker,
		install: true,
	)
endif

dep_libtracker = declare_dependency(
	link_with: lib_libtracker,
	dependencies: deps_libtracker,
)
