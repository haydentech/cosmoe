app_name = 'AboutSystem'
linux_desktop_id = 'x-vnd.Haiku-About'
linux_desktop_categories = 'System;Utility;'

app_srcs = files(
    'AboutSystem.cpp',
    'HyperTextActions.cpp',
    'HyperTextView.cpp',
    'Utilities.cpp',
)

app_resource_defs = files(
    'AboutSystem.rdef',
)

cosmoe_version = run_command('date', '+%F', check: true).stdout().strip()
app_cpp_args = ['-DCOSMOE_VERSION=(built ' + cosmoe_version + ')']
app_dependencies = dep_libbe
app_include_directories = common_inc
app_uses_artwork_resources = true
