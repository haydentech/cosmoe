app_name = 'gui-torture'
linux_desktop_id = 'x-vnd.Cosmoe.gui-torture'
linux_desktop_categories = 'Development;Utility;'

app_srcs = files(
    'app.cpp',
    'disapp.cpp',
    'diswindow.cpp',
    'disview.cpp'
)

app_resource_defs = files(
    app_name + '.rdef'
)

app_link_with = [lib_libbe, lib_libtracker]
app_dependencies = [dep_libbe, dep_libtracker]
app_include_directories = common_inc
