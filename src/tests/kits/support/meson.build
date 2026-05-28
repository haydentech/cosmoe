# Support Kit Test Addon

subdir('remoteobjectdef')

srcs_support_test = files(
  'SupportKitTestAddon.cpp',
  'ByteOrderTest.cpp',
  
  # BArchivable
  'ArchivableTest.cpp',

  # BAutolock
  'AutolockTest.cpp',

  'ByteOrderTest.cpp',
  'DateTimeTest.cpp',
  
  # BLocker
  'LockerConstructionTest.cpp',
  #'LockerConcurrencyTest.cpp',
  #'LockerDestructionTest.cpp',
  #'LockerLockCountTest.cpp',
  
  # BMemoryIO
  'MemoryIOTest.cpp',
  'MallocIOTest.cpp',

  'StopWatchTest.cpp',
  
  # BString
  'StringAccessTest.cpp',
  'StringAppendTest.cpp',
  'StringAssignTest.cpp',
  'StringCaseTest.cpp',
  'StringCompareTest.cpp',
  'StringConstructionTest.cpp',
  'StringEscapeTest.cpp',
  'StringFormatAppendTest.cpp',
  'StringInsertTest.cpp',
  'StringPrependTest.cpp',
  'StringRemoveTest.cpp',
  'StringReplaceTest.cpp',
  'StringSearchTest.cpp',
  'StringSplitTest.cpp',
  'StringSubCopyTest.cpp',
  'StringUTF8Test.cpp',
  
  # BBlockCache
  'BlockCacheExerciseTest.cpp',
  'BlockCacheConcurrencyTest.cpp',
  
  # PointerList
  'PointerListTest.cpp',
)

# Add Windows DLL entry point stub on Windows
if is_windows
  srcs_support_test += files('../test_addon_stub.cpp')
endif

shared_library(
  'supportkittest',
  srcs_support_test,
  include_directories: [common_inc, cppunit_inc],
  dependencies: [dep_libroot, dep_betest],
  link_with: [lib_libbe],
  name_prefix: '',  # Don't add 'lib' prefix
  install: false
)
