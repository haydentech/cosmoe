# libnetservices2 - Network Services 2 static library (C++17)

libnetservices2_sources = files(
    'ErrorsExt.cpp',
    'HttpBuffer.cpp',
    'HttpFields.cpp',
    'HttpParser.cpp',
    'HttpRequest.cpp',
    'HttpResult.cpp',
    'HttpSerializer.cpp',
    'HttpSession.cpp',
    'HttpTime.cpp',
    'NetServicesMisc.cpp',
)

# Create a separate include_directories that puts netservices2 headers FIRST
# This is critical because both netservices and netservices2 have HttpRequest.h
# and we need to find the netservices2 version when compiling libnetservices2
netservices2_inc_dirs = include_directories(
    '.',
    '../../../../headers/private/netservices2',
    is_system: false
)

lib_libnetservices2 = static_library(
    'netservices2',
    libnetservices2_sources,
    include_directories: [netservices2_inc_dirs, common_inc],
    dependencies: [dep_libbe, dep_libnetservices],
    cpp_args: ['-std=gnu++17'],
    install: true,
    install_dir: get_option('libdir'),
)

dep_libnetservices2 = declare_dependency(
    link_with: lib_libnetservices2,
    include_directories: [netservices2_inc_dirs, common_inc],
    dependencies: [dep_libnetservices],
)
