srcs_libtracker = [
    'ContainerWindow.cpp',
    'Model.cpp',
    'FilePanel.cpp',
    'FilePanelPriv.cpp',
    'PoseView.cpp',
    'Settings.cpp',
    'TrackerSettings.cpp',
]

deps_libtracker = [
	dep_libbe,
]

lib_libtracker = shared_library(
	'tracker',
	srcs_libtracker,
	include_directories: common_inc,
	dependencies: deps_libtracker,
	install: false,
)

dep_libtracker = declare_dependency(
	link_with: lib_libtracker,
	dependencies: deps_libtracker,
)
