kernel_misc_file = host_machine.system() == 'darwin' ? 'misc_minimal.c' : 'misc.c'
kernel_sem_file = host_machine.system() == 'darwin' ? 'sem-macos.c' : 'sem.cpp'

# On Windows, add semaphore.c to provide POSIX semaphore wrapper
kernel_extra_files = host_machine.system() == 'windows' ? ['semaphore.c'] : []

srcs_kernel = [
    'image.cpp',
    'port.cpp',
    kernel_sem_file,  # Use sem-macos.c on macOS, sem.cpp elsewhere
    'team.cpp',
    'thread.cpp',
    'misc.c',
    'fs.c',
    'area.c',
    'real_time_clock.c',
] + kernel_extra_files

srcs_kernel = files(srcs_kernel)