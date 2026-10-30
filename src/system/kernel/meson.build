kernel_common_files = [
    'image.cpp',
    'port.cpp',
    'team.cpp',
    'thread.cpp',
    'fs.c',
    'area.c',
    'real_time_clock.c',
	'fs/fd.cpp',
	'fs/vfs.cpp',  # Common platform-agnostic code
]

# Platform-specific pieces, keyed by the machine they are compiled for:
# - macOS: sem-macos.c (uses macOS-specific APIs, standalone)
# - Windows: sem.cpp + semaphore.c (BeOS API + native Windows POSIX wrapper)
# - POSIX: sem.cpp (POSIX semaphores work correctly)
#
# The build-machine list is needed because the tools that run during the build
# link a libbe compiled for the build machine, not the target (see the libbe
# tool library in src/kits).  A cross build to Windows must still get the POSIX
# semaphore implementation for its rc.
kernel_platform_files = {
    'darwin': ['misc_minimal.c', 'sem-macos.c', 'fs/vfs.POSIX.cpp'],
    'windows': ['misc.c', 'sem.cpp', 'semaphore.c', 'fs/vfs.WIN.cpp'],
    'posix': ['misc.c', 'sem.cpp', 'fs/vfs.POSIX.cpp'],
}

host_kernel_key = 'posix'
if is_windows
    host_kernel_key = 'windows'
elif is_darwin
    host_kernel_key = 'darwin'
endif

build_machine_system = build_machine.system()
build_machine_is_windows = build_machine_system == 'windows' \
    or build_machine_system.startswith('msys') or build_machine_system == 'cygwin'
build_kernel_key = 'posix'
if build_machine_is_windows
    build_kernel_key = 'windows'
elif build_machine_system == 'darwin'
    build_kernel_key = 'darwin'
endif

srcs_kernel = files(kernel_common_files + kernel_platform_files[host_kernel_key])
srcs_kernel_build = files(kernel_common_files + kernel_platform_files[build_kernel_key])

# macOS: macmain.mm needs to be in a separate list without common_inc
# to avoid header conflicts with macOS system frameworks
srcs_kernel_macos = []
if host_machine.system() == 'darwin'
    srcs_kernel_macos = files(['macmain.mm'])
endif