kernel_misc_file = host_machine.system() == 'darwin' ? 'misc_minimal.c' : 'misc.c'
kernel_sem_file = host_machine.system() == 'darwin' ? 'sem-macos.c' : 'sem.cpp'

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
]

srcs_kernel = files(srcs_kernel)