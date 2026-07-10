srcs_libtracker = [
	'infowindow/InfoWindow.cpp',
	'infowindow/AttributesView.cpp',
	'infowindow/FilePermissionsView.cpp',
	'infowindow/GeneralInfoView.cpp',
	'infowindow/HeaderView.cpp',
	'AttributeStream.cpp',
	'AutoMounterSettings.cpp',
	'BackgroundImage.cpp',
	'Bitmaps.cpp',
	'ContainerWindow.cpp',
	'CountView.cpp',
	'DeskWindow.cpp',
	'DesktopPoseView.cpp',
	'DraggableContainerIcon.cpp',
	'DialogPane.cpp',
	'DirMenu.cpp',
	'EntryIterator.cpp',
	'FavoritesMenu.cpp',
	'FBCPadding.cpp',
	'FilePanel.cpp',
	'FilePanelPriv.cpp',
	'FindPanel.cpp',
	'FSClipboard.cpp',
	'FSUndoRedo.cpp',
	'FSUtils.cpp',
	'GroupedMenu.cpp',
	'IconCache.cpp',
	'IconMenuItem.cpp',
	'KeyInfos.cpp',
	'LiveMenu.cpp',
	'MimeTypeList.cpp',
	'MiniMenuField.cpp',
	'Model.cpp',
	'MountMenu.cpp',
	'Navigator.cpp',
	'NavMenu.cpp',
	'NodeWalker.cpp',
	'OpenWithWindow.cpp',
	'OverrideAlert.cpp',
	'PendingNodeMonitorCache.cpp',
	'Pose.cpp',
	'PoseList.cpp',
	'PoseView.cpp',
	'PoseViewScripting.cpp',
	'QueryContainerWindow.cpp',
	'QueryPoseView.cpp',
	'RecentItems.cpp',
	'RegExp.cpp',
	'SelectionWindow.cpp',
	'Settings.cpp',
	'SettingsViews.cpp',
	'Shortcuts.cpp',
	'SlowMenu.cpp',
	'StatusWindow.cpp',
	'TaskLoop.cpp',
	'TemplatesMenu.cpp',
	'Tests.cpp',
	'TextWidget.cpp',
	'Thumbnails.cpp',
	'TrashWatcher.cpp',
	'TitleView.cpp',
	'TrackerInit.cpp',
	'Tracker.cpp',
	'TrackerInitialState.cpp',
	'TrackerScripting.cpp',
	'TrackerString.cpp',
	'TrackerSettingsWindow.cpp',
	'TrackerSettings.cpp',
	'Utilities.cpp',
	'ViewState.cpp',
	'VirtualDirectoryEntryList.cpp',
	'VirtualDirectoryManager.cpp',
	'VirtualDirectoryPoseView.cpp',
	'VirtualDirectoryWindow.cpp',
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
		[srcs_libtracker, libtracker_resources_obj],
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
		[srcs_libtracker, libtracker_resources_obj],
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
	include_directories: tracker_inc,
)
