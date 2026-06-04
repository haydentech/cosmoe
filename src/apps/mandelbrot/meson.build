app_name = 'Mandelbrot'
linux_desktop_id = 'x-vnd.Haiku-Mandelbrot'
linux_desktop_categories = 'Graphics;Science;'

app_srcs = files(
    'FractalEngine.cpp',
    'Mandelbrot.cpp',
)


app_resource_defs = files(
    app_name + '.rdef'
)

app_link_with = [lib_libbe, lib_libtracker]
app_dependencies = [dep_libtracker]
app_include_directories = common_inc
