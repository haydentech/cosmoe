# NetAPI Kit Test Addon

srcs_netservices2_test = files(
  'ServicesKitTestAddon.cpp',
  'ExclusiveBorrowTest.cpp',
#   'HttpDebugLogger.cpp',
#   'HttpProtocolTest.cpp',
#   'TestServer.cpp',
)

# Add Windows DLL entry point stub on Windows
if is_windows
  srcs_netservices2_test += files('../../test_addon_stub.cpp')
endif

shared_library(
  'netservices2kittest',
  srcs_netservices2_test,
  include_directories: [common_inc, cppunit_inc],
  dependencies: [dep_libroot, dep_betest],
  link_with: [lib_libbe, lib_libnetapi, lib_libnetservices2],
  name_prefix: '',  # Don't add 'lib' prefix
  install: false
)
