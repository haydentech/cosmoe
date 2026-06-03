# Windows backend for Cosmoe
# This backend provides native Windows support using Windows APIs
# Structure follows X11 backend: C window API + C++ wrapper

windows_sources = files(
    'window.c',              # C implementation of window/widget API
    'WindowsBackend.cpp',    # C++ backend wrapper
    'WindowsBackendInit.cpp',
    'debug_log.c',           # Debug logging for Windows
)

# xkbcommon is not available on Windows - Windows has its own keyboard APIs
xkbcommon_dep = dependency('xkbcommon', required: false)
fontconfig_dep = dependency('fontconfig', required: true)
pango_dep = dependency('pango', required: true)
pangocairo_dep = dependency('pangocairo', required: true)
windows_deps = [dependency('cairo'), fontconfig_dep, pango_dep, pangocairo_dep]
if xkbcommon_dep.found()
    windows_deps += xkbcommon_dep
endif

# Windows-specific libraries
windows_link_args = [
    '-lgdi32',      # GDI (Graphics Device Interface)
    '-luser32',     # Window management
    '-lkernel32',   # Core Windows APIs
    '-lcomdlg32',   # Common dialogs (file open/save, etc.)
]

shared_library(
    'cosmoe-windows',
    windows_sources,
    include_directories: [include_directories('.'), common_inc],
    dependencies: windows_deps + [dep_libbe],
    link_args: windows_link_args,
    install: true,
    c_args: ['-DBUILDING_COSMOE_WINDOWS=1'],
    cpp_args: ['-DBUILDING_COSMOE_WINDOWS=1'],
    name_prefix: 'lib',
)
