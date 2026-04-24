kernel_misc_file = host_machine.system() == 'darwin' ? 'misc_minimal.c' : 'misc.c'

# Semaphore implementation selection:
# - macOS: sem-macos.c (uses macOS-specific APIs, standalone)
# - Windows: sem.cpp + semaphore.c (BeOS API + native Windows POSIX wrapper)
# - Linux: sem.cpp (POSIX semaphores work correctly)
if host_machine.system() == 'darwin'
    kernel_sem_files = ['sem-macos.c']
elif host_machine.system() == 'windows'
    kernel_sem_files = ['sem.cpp', 'semaphore.c']
else
    kernel_sem_files = ['sem.cpp']
endif

srcs_kernel = [
    'image.cpp',
    'port.cpp',
    'team.cpp',
    'thread.cpp',
    'misc.c',
    'fs.c',
    'area.c',
    'real_time_clock.c',
	'fs/fd.cpp',
	'fs/vfs.cpp',  # Common platform-agnostic code
] + kernel_sem_files

# Add platform-specific kernel interface implementation
if is_windows
    srcs_kernel += ['fs/vfs.WIN.cpp']
else
    srcs_kernel += ['fs/vfs.POSIX.cpp']
endif

srcs_kernel = files(srcs_kernel)

# macOS: macmain.mm needs to be in a separate list without common_inc
# to avoid header conflicts with macOS system frameworks
srcs_kernel_macos = []
if host_machine.system() == 'darwin'
    srcs_kernel_macos = files(['macmain.mm'])
endif