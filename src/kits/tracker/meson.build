srcs_libtracker = [
	'infowindow/InfoWindow.cpp',
	'infowindow/AttributesView.cpp',
	'infowindow/FilePermissionsView.cpp',
	'infowindow/GeneralInfoView.cpp',
	'infowindow/HeaderView.cpp',
	'Bitmaps.cpp',
	'ContainerWindow.cpp',
	'EntryIterator.cpp',
	'FBCPadding.cpp',
	'FilePanelFileColumn.cpp',
	'FilePanelPoseView.cpp',
	'FilePanel.cpp',
	'FilePanelPriv.cpp',
	'FSClipboard.cpp',
	'FSUtils.cpp',
	'IconCache.cpp',
	'IconMenuItem.cpp',
	'Model.cpp',
	'Navigator.cpp',
	'NavMenu.cpp',
	'Pose.cpp',
	'PoseList.cpp',
	'PoseView.cpp',
	'RegExp.cpp',
	'Shortcuts.cpp',
	'SlowMenu.cpp',
	'Thumbnails.cpp',
	'TrackerInit.cpp',
	'Tracker.cpp',
	'Utilities.cpp',
	'WidgetAttributeText.cpp'
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
	lib_libtracker = shared_library(
		'tracker',
		srcs_libtracker,
		cpp_args: ['-D_SUPPORTS_RESOURCES=1'],
		include_directories: [common_inc, tracker_inc],
		install_dir: dir_lib,
		dependencies: deps_libtracker,
		link_args: ['-Wl,-sectcreate,__TEXT,__rsrc,' + meson.current_build_dir() / 'libtracker.rsrc'],
		link_depends: [libtracker_rsrc],
		install: true,
	)
elif is_linux
	# Linux: Embed full resource file (with RS magic) as binary data in ELF
	libtracker_resources_obj = custom_target('libtracker_resources_obj',
		input: libtracker_rsrc,
		output: 'libtracker_rsrc.o',
		command: [
			'sh', '-c',
			'OUTPUT_ABS=$(cd $(dirname "$2") && pwd)/$(basename "$2") &&' +
			# Create object with embedded binary, add .note.GNU-stack, and rename symbols
			'ld -r -b binary "$1" -o "$2" && ' +
			# Rename .data section to .cosmoe_rsrc so it doesn't merge with executable's .data
			# which can prevent us from finding it at runtime
			'objcopy --rename-section .data=.cosmoe_rsrc "$OUTPUT_ABS" && ' +
			'objcopy ' +
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
		install_dir: dir_lib,
		dependencies: deps_libtracker,
		install: true,
	)
else
	# Windows: Use objcopy to embed resources in PE/COFF format
	libtracker_resources_obj = custom_target('libtracker_resources_obj',
		input: libtracker_rsrc,
		output: 'libtracker_rsrc.o',
		command: [
			'sh', '-c',
			'objcopy -I binary -O pe-x86-64 -B i386:x86-64 ' +
			'--rename-section .data=.cosmoe_rsrc,alloc,load,readonly,data,contents ' +
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
		install_dir: dir_lib,
		dependencies: deps_libtracker,
		install: true,
	)
endif

dep_libtracker = declare_dependency(
	link_with: lib_libtracker,
	dependencies: deps_libtracker,
)
