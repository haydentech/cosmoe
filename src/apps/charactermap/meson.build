app_name = 'CharacterMap'
linux_desktop_id = 'x-vnd.Haiku-CharacterMap'
linux_desktop_categories = 'Education;Utility;'

app_srcs = files(
    'CharacterMap.cpp',
    'CharacterView.cpp',
    'CharacterWindow.cpp',
    'UnicodeBlocks.cpp',
	'UnicodeBlockView.cpp'
)

app_resource_defs = files(
    app_name + '.rdef'
)

app_dependencies = [dep_libbe, dep_libtracker]
app_include_directories = [common_inc, tracker_inc]
