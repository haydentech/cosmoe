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
	# Linux: Embed full resource file (with RS magic) as binary data in ELF
	# Use objcopy to rename symbols to standard names expected by ResourceFile.cpp
	libtracker_resources_obj = custom_target('libtracker_resources_obj',
		input: libtracker_rsrc,
		output: 'libtracker_rsrc.o',
		command: [
			'sh', '-c',
			# Create object with embedded binary, add .note.GNU-stack, and rename symbols
			'ld -r -b binary "$1" -o "$2" && ' +
			'objcopy ' +
			'--redefine-sym _binary_libtracker_rsrc_start=_binary_app_rsrc_start ' +
			'--redefine-sym _binary_libtracker_rsrc_end=_binary_app_rsrc_end ' +
			'--redefine-sym _binary_libtracker_rsrc_size=_binary_app_rsrc_size ' +
			'--add-section .note.GNU-stack=/dev/null ' +
			'--set-section-flags .note.GNU-stack=noload,readonly "$2"',
			'--', '@INPUT@', '@OUTPUT@'
		],
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
	# Windows: Use objcopy to embed resources in PE/COFF format
	libtracker_rsrc_stripped = custom_target('libtracker_rsrc_stripped',
		input: libtracker_rsrc,
		output: 'libtracker.rsrc.stripped',
		command: ['dd', 'if=@INPUT@', 'of=@OUTPUT@', 'bs=4', 'skip=1', 'status=none'],
	)
	
	libtracker_resources_obj = custom_target('libtracker_resources_obj',
		input: libtracker_rsrc_stripped,
		output: 'libtracker_rsrc.o',
		command: [
			'sh', '-c',
			'objcopy -I binary -O pe-x86-64 -B i386:x86-64 ' +
			'--rename-section .data=.cosmoe_rsrc,alloc,load,readonly,data,contents ' +
			'--redefine-sym _binary_libtracker_rsrc_stripped_start=_binary_lib_rsrc_start ' +
			'--redefine-sym _binary_libtracker_rsrc_stripped_end=_binary_lib_rsrc_end ' +
			'--redefine-sym _binary_libtracker_rsrc_stripped_size=_binary_lib_rsrc_size ' +
			'"$1" "$2"',
			'--', '@INPUT@', '@OUTPUT@'
		],
		build_by_default: true,
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
endif

dep_libtracker = declare_dependency(
	link_with: lib_libtracker,
	dependencies: deps_libtracker,
)
