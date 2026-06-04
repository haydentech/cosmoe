app_name = 'minimal'
linux_desktop_id = 'x-vnd.minimal'
linux_desktop_categories = 'Development;'

app_srcs = files(
    'app.cpp',
    'disapp.cpp',
    'diswindow.cpp',
    'disview.cpp'
)

app_resource_defs = files(
    app_name + '.rdef'
)

app_link_with = [lib_libbe]
app_dependencies = [dep_libbe]
app_include_directories = common_inc
app_link_args = is_windows ? ['-Wl,--enable-runtime-pseudo-reloc', '-Wl,--enable-auto-import'] : []
