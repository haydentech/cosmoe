# Locale Kit Test Addon

srcs_locale_test = files(
  'LocaleKitTestAddon.cpp',
  'CollatorTest.cpp',
  'DateFormatTest.cpp',
  'DurationFormatTest.cpp',
  'LanguageTest.cpp',
  'NumberFormatTest.cpp',
  'RelativeDateTimeFormatTest.cpp',
  'StringFormatTest.cpp',
  'UnicodeCharTest.cpp',
)

# Add Windows DLL entry point stub on Windows
if is_windows
  srcs_locale_test += files('../test_addon_stub.cpp')
endif

shared_library(
  'localekittest',
  srcs_locale_test,
  include_directories: [common_inc, cppunit_inc],
  dependencies: [dep_libroot, dep_betest],
  link_with: [lib_libbe],
  name_prefix: '',  # Don't add 'lib' prefix
  install: false
)


# executable(
#   'ICUTest',
#   'ICUTest.cpp',
#   include_directories: common_inc,
#   dependencies: dep_libroot,
#   link_with: lib_libbe,
#   install: false
# )

# executable(
#   'catalogTest',
#   'catalogTest.cpp',
#   include_directories: common_inc,
#   dependencies: dep_libroot,
#   link_with: [lib_libbe, lib_localestub],
#   install: false
# )