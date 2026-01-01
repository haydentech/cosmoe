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

lib_libtracker = shared_library(
	'tracker',
	srcs_libtracker,
	cpp_args: ['-D_SUPPORTS_RESOURCES=1'],
	include_directories: [common_inc, tracker_inc],
    install_dir: '/usr/local/lib',
	dependencies: deps_libtracker,
	install: true,
)

dep_libtracker = declare_dependency(
	link_with: lib_libtracker,
	dependencies: deps_libtracker,
)
