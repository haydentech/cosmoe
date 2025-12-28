# Windows backend for Cosmoe
# This backend provides native Windows support using Windows APIs
# Structure follows X11 backend: C window API + C++ wrapper

windows_sources = files(
    'window.c',              # C implementation of window/widget API
    'WindowsBackend.cpp',    # C++ backend wrapper
)

windows_deps = [
    deps_cairo_shared,
    dependency('xkbcommon'),  # For keyboard handling (cross-platform)
]

# Windows-specific libraries
windows_link_args = []
if host_system == 'windows'
    windows_link_args += [
        '-lgdi32',      # GDI (Graphics Device Interface)
        '-luser32',     # Window management
        '-lkernel32',   # Core Windows APIs
        '-lcomdlg32',   # Common dialogs (file open/save, etc.)
    ]
endif

libcosmoe_windows = shared_library(
    'cosmoe-windows',
    windows_sources,
    include_directories: inc,
    dependencies: windows_deps,
    link_args: windows_link_args,
    install: true,
    c_args: ['-DBUILDING_COSMOE_WINDOWS=1'],
    cpp_args: ['-DBUILDING_COSMOE_WINDOWS=1'],
)
