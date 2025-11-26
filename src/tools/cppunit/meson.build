# CppUnit test framework library (libbetest)

cppunit_inc = include_directories(
  '../../../headers/tools/cppunit'
)

# Use system cppunit library
dep_cppunit = dependency('cppunit', required: true)

srcs_libbetest = files(
  'BTestCase.cpp',
  'BTestSuite.cpp',
  'SemaphoreSyncObject.cpp',
  'TestApp.cpp',
  'TestListener.cpp',
  'TestShell.cpp',
  'TestUtils.cpp',
  'ThreadedTestCase.cpp',
)

lib_betest = shared_library(
  'betest',
  srcs_libbetest,
  include_directories: [common_inc, cppunit_inc],
  dependencies: [dep_libroot, dep_cppunit],
  link_with: [lib_libbe],
  install: true,
  install_dir: get_option('libdir')
)

dep_betest = declare_dependency(
  link_with: lib_betest,
  include_directories: cppunit_inc,
  dependencies: [dep_cppunit]
)
