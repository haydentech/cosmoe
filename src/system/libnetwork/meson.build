# libnetwork - System network library

# Build subdirectories first (object libraries)
#subdir('musl')
#subdir('netresolv')

libnetwork_sources = files(
    'init.cpp',
    'interfaces.cpp',
    'getifaddrs.cpp',
    #'socket.cpp',
    #'r5_compatibility.cpp',
)

libnetwork_inc = include_directories('.')

# libnetwork.so - shared library
lib_libnetwork = shared_library(
    'network',
    libnetwork_sources,
    include_directories: [common_inc, libnetwork_inc],
    #link_whole: [musl_network_lib, netresolv_lib],
    cpp_args: ['-std=gnu++11'],
    install: true,
    install_dir: get_option('libdir'),
    version: version_cosmoe,
    soversion: '0',
)

dep_libnetwork = declare_dependency(
    link_with: lib_libnetwork,
    include_directories: [common_inc, libnetwork_inc],
)
