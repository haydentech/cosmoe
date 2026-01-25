# libnetapi - Network API shared library

libnetapi_sources = files(
    'DynamicBuffer.cpp',
    'NetEndpoint.cpp',
    'NetAddress.cpp',
    'NetBuffer.cpp',
    'NetDebug.cpp',
    'Certificate.cpp',
    'NetworkAddress.cpp',
    'NetworkAddressResolver.cpp',
    #'NetworkDevice.cpp',
    #'NetworkInterface.cpp',
    #'NetworkRoster.cpp',
    #'NetworkRoute.cpp',
    #'NetworkSettings.cpp',
    'AbstractSocket.cpp',
    'DatagramSocket.cpp',
    'ProxySecureSocket.cpp',
    'SecureSocket.cpp',
    'Socket.cpp',
    'SocketMessenger.cpp',
    #'notifications.cpp',
)

libnetapi_inc = include_directories('.')

# Check for OpenSSL
dep_openssl = dependency('openssl', version: '>= 1.1.0', required: false)

libnetapi_cpp_args = []
libnetapi_deps = [dep_libbe]

# Add Windows socket library
if host_machine.system() == 'windows'
    libnetapi_deps += meson.get_compiler('cpp').find_library('ws2_32')
endif

if dep_openssl.found()
    libnetapi_cpp_args += [
        '-DOPENSSL_ENABLED',
        '-DOPENSSL_API_COMPAT=10101',
        '-DOPENSSL_NO_DEPRECATED=1'
    ]
    libnetapi_deps += dep_openssl
endif

lib_libnetapi = shared_library(
    'bnetapi',
    libnetapi_sources,
    include_directories: [common_inc, libnetapi_inc],
    dependencies: libnetapi_deps,
    cpp_args: libnetapi_cpp_args,
    install: true,
    install_dir: get_option('libdir'),
    version: version_cosmoe,
    soversion: '0',
)

dep_libnetapi = declare_dependency(
    link_with: lib_libnetapi,
    include_directories: [common_inc, libnetapi_inc],
    dependencies: libnetapi_deps,
)
