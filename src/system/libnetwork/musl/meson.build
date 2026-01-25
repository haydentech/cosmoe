# musl network - POSIX network functions from musl libc

musl_network_sources = files(
    'network/inet_addr.c',
    'network/inet_aton.c',
    'network/inet_legacy.c',
    'network/inet_ntoa.c',
    'network/inet_ntop.c',
    'network/inet_pton.c',
)

# Build as object library to be linked into libnetwork.so
musl_network_lib = static_library(
    'posix_musl_network',
    musl_network_sources,
    include_directories: common_inc,
    pic: true,  # Position independent code for shared library
)
