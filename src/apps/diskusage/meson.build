app_name = 'DiskUsage'
linux_desktop_id = 'x-vnd.Haiku-DiskUsage'
linux_desktop_categories = 'System;Filesystem;Utility;'

app_srcs = files(
    'App.cpp',
	'ControlsView.cpp',
	'DiskUsage.cpp',
	'InfoWindow.cpp',
	'MainWindow.cpp',
	'PieView.cpp',
	'Scanner.cpp',
	'Snapshot.cpp',
	'StatusView.cpp',
	'VolumeView.cpp'
)


app_resource_defs = files(
    app_name + '.rdef'
)

app_dependencies = dep_libbe
app_include_directories = [common_inc, tracker_inc]
