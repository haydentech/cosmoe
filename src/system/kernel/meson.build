kernel_misc_file = host_machine.system() == 'darwin' ? 'misc_minimal.c' : 'misc.c'

srcs_kernel = [
    'image.cpp',
    'port.cpp',
    'sem.cpp',
    'team.cpp',
    'thread.cpp',
    kernel_misc_file,  # Use misc_minimal.c on macOS to avoid duplicate symbols
    'fs.c',
    'area.c',
    'real_time_clock.c',
]

srcs_kernel = files(srcs_kernel)