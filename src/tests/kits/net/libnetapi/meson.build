# NetAPI Kit Test Addon

srcs_netapi_test = files(
  'NetAPITestAddon.cpp',
  'NetworkAddressTest.cpp',
#  'NetworkInterfaceTest.cpp',
  'NetworkUrlTest.cpp',
)

# Add Windows DLL entry point stub on Windows
netapi_test_deps = [dep_libroot, dep_betest]

if is_windows
  srcs_netapi_test += files('../../test_addon_stub.cpp')
  netapi_test_deps += meson.get_compiler('cpp').find_library('ws2_32')
endif

shared_library(
  'netapikittest',
  srcs_netapi_test,
  include_directories: [common_inc, cppunit_inc],
  dependencies: netapi_test_deps,
  link_with: [lib_libbe, lib_libnetapi],
  name_prefix: '',  # Don't add 'lib' prefix
  install: false
)
