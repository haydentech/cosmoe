app_name = 'msgtest'
linux_desktop_id = 'x-vnd.Cosmoe-msgtest'
linux_desktop_categories = 'Development;Utility;'

app_srcs = files(
    'diswindow.cpp',
    'disapp.cpp',
    'app.cpp',
)


app_resource_defs = files(
    app_name + '.rdef'
)

# AboutSystem has already moved the artwork to the build directory so the resource compiler can find it

app_dependencies = [dep_libbe, dep_libtracker]
