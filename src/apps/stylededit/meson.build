app_name = 'StyledEdit'
linux_desktop_id = 'x-vnd.Haiku-StyledEdit'
linux_desktop_categories = 'Utility;TextEditor;'

app_srcs = files(
    'ColorMenuItem.cpp',
    'FindWindow.cpp',
    'ReplaceWindow.cpp',
    'StatusView.cpp',
    'StyledEditApp.cpp',
    'StyledEditView.cpp',
    'StyledEditWindow.cpp',
)

app_resource_defs = files(
    app_name + '.rdef'
)

app_dependencies = [dep_libbe, dep_libtracker]
app_include_directories = [common_inc, tracker_inc]
