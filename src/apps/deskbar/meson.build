app_name = 'Deskbar'
linux_desktop_id = 'x-vnd.Be-TSKB'
linux_desktop_categories = 'System;Utility;'

app_srcs = files(
    'BarApp.cpp',
    'BarMenuBar.cpp',
    'BarMenuTitle.cpp',
    'BarView.cpp',
    'BarWindow.cpp',
    'DeskbarMenu.cpp',
    'DeskbarUtils.cpp',
    'ExpandoMenuBar.cpp',
    'InlineScrollView.cpp',
    'LauncherView.cpp',
    'PreferencesWindow.cpp',
    'ShowHideMenuItem.cpp',
    'StatusView.cpp',
    'StatusViewShelf.cpp',
    'TeamMenu.cpp',
    'TeamMenuItem.cpp',
    'TimeView.cpp',
    'TruncatableMenuItem.cpp',
    'WindowMenu.cpp',
    'WindowMenuItem.cpp',
    'ResourceSet.cpp',
    'Switcher.cpp',
    'CalendarMenuWindow.cpp',
	'ScreenCornerSelector.cpp',
)

app_resource_defs = files(
    'Deskbar.rdef',
    'icons.rdef',
)

app_dependencies = [dep_libbe, dep_libtracker]
app_include_directories = [common_inc, tracker_inc]
app_cpp_args = [
    '-DDB_ADDONS',
    '-D_INCLUDES_CLASS_DEVICE_MAP=1',
    '-D_SUPPORTS_RESOURCES=1',
    '-D_SUPPORTS_FEATURE_SCRIPTING=1',
    '-D_SHOW_CALENDAR_MENU_WINDOW=1',
]
