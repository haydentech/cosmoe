# libnetservices - Network Services static library

libnetservices_sources = files(
    'DataRequest.cpp',
    'HttpAuthentication.cpp',
    'HttpHeaders.cpp',
    'HttpForm.cpp',
    'HttpRequest.cpp',
    'HttpResult.cpp',
    'FileRequest.cpp',
    'GopherRequest.cpp',
    'Geolocation.cpp',
    'HttpTime.cpp',
    'NetworkCookie.cpp',
    'NetworkCookieJar.cpp',
    'NetworkRequest.cpp',
    'UrlContext.cpp',
    'UrlProtocolAsynchronousListener.cpp',
    'UrlProtocolDispatchingListener.cpp',
    'UrlProtocolListener.cpp',
    'UrlProtocolRoster.cpp',
    'UrlRequest.cpp',
    'UrlResult.cpp',
    'UrlSynchronousRequest.cpp',
)

# Include netservices headers directory for this library
netservices_inc_dirs = include_directories(
    '.',
    '../../../../headers/private/netservices',
    is_system: false
)

lib_libnetservices = static_library(
    'netservices',
    libnetservices_sources,
    include_directories: [netservices_inc_dirs, common_inc],
    dependencies: [dep_libbe],
    install: true,
    install_dir: get_option('libdir'),
)

dep_libnetservices = declare_dependency(
    link_with: lib_libnetservices,
    include_directories: [netservices_inc_dirs, common_inc],
)
