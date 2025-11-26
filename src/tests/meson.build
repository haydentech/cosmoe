# Tests
subdir('system')
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
  link_with: [lib_libbe],
  install: true,
  install_dir: get_option('bindir')
)

