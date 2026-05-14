# Tests
subdir('system')
subdir('servers/app')

# Only build kit tests and UnitTester on non-Windows (they need cppunit)
if not is_windows and have_betest
  subdir('kits')

  # Build the main UnitTester executable
  srcs_unittester = files(
    'UnitTester.cpp',
  )

  executable(
    'UnitTester',
    srcs_unittester,
    include_directories: [common_inc, cppunit_inc],
    dependencies: [dep_libroot, dep_betest],
    link_with: [lib_libbe, lib_localestub],
    install: false
  )
elif not is_windows and unit_tests_enabled
  message('cppunit not found - skipping kit tests and UnitTester')
endif

# BPath Windows Test - standalone test for Windows path handling
if is_windows
  executable(
    'BPathTest',
    'kits/storage/BPathTest.cpp',
    include_directories: common_inc,
    dependencies: dep_libroot,
    link_with: [lib_libbe, lib_localestub],
    install: false,
  )
endif
