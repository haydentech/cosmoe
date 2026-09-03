executable(
	'chart',
	['Chart.cpp', 'ChartRender.cpp', 'ChartView.cpp', 'ChartWindow.cpp'],
	dependencies: [dep_libroot, dep_libbe, dep_libgame, dep_libtracker],
	include_directories: common_inc,
	link_with: [lib_libbe, lib_libgame, lib_libtracker],
	install: false,
)