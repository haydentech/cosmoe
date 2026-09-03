app_name = 'Chart'
linux_desktop_id = 'x-vnd.Haiku-Chart'
linux_desktop_categories = 'Graphics;Utility;'

app_srcs = files(
    'Chart.cpp',
    'ChartRender.cpp',
    'ChartView.cpp',
    'ChartWindow.cpp',
)

app_resource_defs = files(
    app_name + '.rdef'
)

app_link_with = [lib_libbe, lib_libgame]
app_dependencies = [dep_libbe, dep_libgame]
app_include_directories = common_inc
