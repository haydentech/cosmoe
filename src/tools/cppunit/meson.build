# CppUnit test framework library (libbetest)

cppunit_inc = include_directories(
  '../../../headers/tools/cppunit'
)

have_betest = false
dep_betest = disabler()

srcs_libcppunit = files(
  'cppunit/Asserter.cpp',
  'cppunit/CompilerOutputter.cpp',
  'cppunit/Exception.cpp',
  'cppunit/NotEqualException.cpp',
  'cppunit/RepeatedTest.cpp',
  'cppunit/SourceLine.cpp',
  'cppunit/SynchronizedObject.cpp',
  'cppunit/TestAssert.cpp',
  'cppunit/TestCase.cpp',
  'cppunit/TestFactoryRegistry.cpp',
  'cppunit/TestFailure.cpp',
  'cppunit/TestResult.cpp',
  'cppunit/TestResultCollector.cpp',
  'cppunit/TestRunner.cpp',
  'cppunit/TestSetUp.cpp',
  'cppunit/TestSucessListener.cpp',
  'cppunit/TestSuite.cpp',
  'cppunit/TextOutputter.cpp',
  'cppunit/TextTestProgressListener.cpp',
  'cppunit/TextTestResult.cpp',
  'cppunit/TypeInfoHelper.cpp',
  'cppunit/XmlOutputter.cpp',
)

lib_cppunit = static_library(
  'cppunit',
  srcs_libcppunit,
  include_directories: [common_inc, cppunit_inc],
  dependencies: [dep_libroot],
  cpp_args: ['-DBUILDING_CPPUNIT'],
  pic: true,
  install: false
)

dep_cppunit = declare_dependency(
  link_with: lib_cppunit,
  include_directories: cppunit_inc
)

srcs_libbetest = files(
  'BTestCase.cpp',
  'BTestSuite.cpp',
  'SemaphoreSyncObject.cpp',
  'TestApp.cpp',
  'TestListener.cpp',
  'TestShell.cpp',
  'TestUtils.cpp',
  'ThreadedTestCase.cpp',
  'BetestInit.cpp',
)

lib_betest = shared_library(
  'betest',
  srcs_libbetest,
  include_directories: [common_inc, cppunit_inc],
  dependencies: [dep_libroot],
  link_whole: [lib_cppunit],
  link_with: [lib_libbe],
  install: true,
  install_dir: get_option('libdir')
)

dep_betest = declare_dependency(
  link_with: lib_betest,
  include_directories: cppunit_inc
)
have_betest = true
