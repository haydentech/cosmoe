# Support Kit Test Addon

srcs_support_test = files(
  'SupportKitTestAddon.cpp',
  'ByteOrderTest.cpp',
  
  # BArchivable
  'barchivable/ArchivableTest.cpp',
  'barchivable/BArchivableTester.cpp',
  #'barchivable/FindInstantiationFuncTester.cpp',
  #'barchivable/InstantiateObjectTester.cpp',	// Roster issues
  'barchivable/LocalTestObject.cpp',
  'barchivable/ValidateInstantiationTester.cpp',
  
  # BAutolock - TODO: Requires BThreadedTestCaller compatibility
  # 'bautolock/AutolockTest.cpp',
  # 'bautolock/AutolockLockerTest.cpp',
  # 'bautolock/AutolockLooperTest.cpp',
  
  # BLocker - TODO: Requires BThreadedTestCaller compatibility  
  # 'blocker/LockerTest.cpp',
  # 'blocker/BenaphoreLockCountTest1.cpp',
  # 'blocker/ConcurrencyTest1.cpp',
  # 'blocker/ConcurrencyTest2.cpp',
  # 'blocker/ConstructionTest1.cpp',
  # 'blocker/DestructionTest1.cpp',
  # 'blocker/DestructionTest2.cpp',
  # 'blocker/LockerTestCase.cpp',
  # 'blocker/SemaphoreLockCountTest1.cpp',
  
  # BMemoryIO
  'bmemoryio/MemoryIOTest.cpp',
  'bmemoryio/ConstTest.cpp',
  'bmemoryio/SeekTest.cpp',
  'bmemoryio/WriteTest.cpp',
  'bmemoryio/ReadTest.cpp',
  'bmemoryio/SetSizeTest.cpp',
  
  # BMallocIO
  'bmemoryio/MallocIOTest.cpp',
  'bmemoryio/MallocSeekTest.cpp',
  'bmemoryio/MallocWriteTest.cpp',
  'bmemoryio/MallocBufferLengthTest.cpp',
  
  # BString
  'bstring/StringTest.cpp',
  'bstring/StringConstructionTest.cpp',
  'bstring/StringAccessTest.cpp',
  'bstring/StringAssignTest.cpp',
  'bstring/StringAppendTest.cpp',
  'bstring/StringSubCopyTest.cpp',
  'bstring/StringPrependTest.cpp',
  'bstring/StringCaseTest.cpp',
  'bstring/StringInsertTest.cpp',
  'bstring/StringEscapeTest.cpp',
  'bstring/StringRemoveTest.cpp',
  'bstring/StringCompareTest.cpp',
  'bstring/StringFormatAppendTest.cpp',
  # 'bstring/StringCharAccessTest.cpp',  # TODO: BString char operator[] not implemented
  'bstring/StringSearchTest.cpp',
  'bstring/StringReplaceTest.cpp',
  'bstring/StringSplitTest.cpp',
  
  # BBlockCache
  'bblockcache/BlockCacheTest.cpp',
  'bblockcache/BlockCacheExerciseTest.cpp',
  # 'bblockcache/BlockCacheConcurrencyTest.cpp',  # TODO: Requires BThreadedTestCaller compatibility
  
  # PointerList - TODO: _PointerList_::Owning() not implemented, missing printf include
  # 'pointerlist/PointerListTest.cpp',
  'pointerlist/PointerListSortTest.cpp',
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
