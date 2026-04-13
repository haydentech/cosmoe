# Tests
subdir('system')

# Only build kit tests and UnitTester on non-Windows (they need cppunit)
if not is_windows
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
