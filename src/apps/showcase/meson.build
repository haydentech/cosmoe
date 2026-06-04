app_name = 'Showcase'
linux_desktop_id = 'x-vnd.Cosmoe-Showcase'
linux_desktop_categories = 'Development;Graphics;'

app_srcs = files(
    'diswindow.cpp',
    'disview.cpp',
    'IconTab.cpp',
    'IconTabView.cpp',
    'IconOutlineListView.cpp',
    'disapp.cpp',
    'app.cpp',
	'Placeholder.cpp',
	'VectorImageButton.cpp'
)

app_resource_defs = files(
    app_name + '.rdef'
)

app_dependencies = [dep_libbe, dep_libtracker]
app_include_directories = [common_inc, tracker_inc]
app_uses_artwork_resources = true
