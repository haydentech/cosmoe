# Backend plugins for Cosmoe
#
# This directory is intentionally built AFTER src/kits so that backends can
# link against dep_libbe (the real libbe.so / libbe.dll) without any
# build-order or circular-dependency issues.

if wayland_enabled
    subdir('wayland')
    subdir('wayland/backend')
endif

if x11_enabled
    subdir('X11')
endif

if cocoa_enabled
    subdir('cocoa')
endif

if windows_enabled
    subdir('windows')
endif
