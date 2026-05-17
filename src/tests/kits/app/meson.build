# App Kit MessageRunner Test Addon

srcs_app_test = files(
  'bmessagerunner/MessageRunnerTest.cpp',
  'bmessagerunner/MessageRunnerTestHelpers.cpp',
  'bmessagerunner/BMessageRunnerTester.cpp',
  'bpropertyinfo/PropertyConstructionTest.cpp',
  'bpropertyinfo/PropertyFindMatchTest.cpp',
  'bpropertyinfo/PropertyFlattenTest.cpp',
  'bpropertyinfo/PropertyInfoTest.cpp',
  'bpropertyinfo/PropertyTestcase.cpp',
  'bmessagequeue/MessageQueueTest.cpp',
  'bmessagequeue/MessageQueueTestCase.cpp',
  'bmessagequeue/AddMessageTest1.cpp',
  'bmessagequeue/AddMessageTest2.cpp',
  'bmessagequeue/FindMessageTest1.cpp',
  'bmessagequeue/MessageQueueTestAddon.cpp',
  'bmessage/MessageTest.cpp',
  'bmessage/MessageConstructTest.cpp',
  'bmessage/MessageDestructTest.cpp',
  'bmessage/MessageEasyFindTest.cpp',
  'bmessage/MessageInt32ItemTest.cpp',
  'bmessage/MessageOpAssignTest.cpp',
  'bmessage/MessageSpeedTest.cpp',
  'bhandler/AddFilterTest.cpp',
  'bhandler/BHandlerTester.cpp',
  'bhandler/HandlerLooperTest.cpp',
  'bhandler/HandlerTest.cpp',
  'bhandler/IsWatchedTest.cpp',
  'bhandler/LockLooperTest.cpp',
  'bhandler/LockLooperTestCommon.cpp',
  'bhandler/LockLooperWithTimeoutTest.cpp',
  'bhandler/NextHandlerTest.cpp',
  'bhandler/RemoveFilterTest.cpp',
  'bhandler/SetFilterListTest.cpp',
  'bhandler/SetNextHandlerTest.cpp',
  'bhandler/UnlockLooperTest.cpp',
  'blooper/AddHandlerTest.cpp',
  'blooper/AddCommonFilterTest.cpp',
  'blooper/CountHandlersTest.cpp',
  'blooper/HandlerAtTest.cpp',
  'blooper/IndexOfTest.cpp',
  'blooper/IsMessageWaitingTest.cpp',
  'blooper/LooperForThreadTest.cpp',
  'blooper/LooperSizeTest.cpp',
  'blooper/LooperTest.cpp',
  'blooper/PerformTest.cpp',
  'blooper/QuitTest.cpp',
  'blooper/RemoveCommonFilterTest.cpp',
  'blooper/RemoveHandlerTest.cpp',
  'blooper/RunTest.cpp',
  'blooper/SetCommonFilterListTest.cpp',

  'AppKitTestAddon.cpp',
  'AppKitTestStub.cpp',
)

# Add Windows DLL entry point stub on Windows
if is_windows
  srcs_app_test += files('../test_addon_stub.cpp')
endif

shared_library(
  'appkittest',
  srcs_app_test,
  include_directories: [common_inc, cppunit_inc],
  dependencies: [dep_libroot, dep_betest],
  link_with: [lib_libbe],
  name_prefix: '',  # Don't add 'lib' prefix
  install: false
)

executable(
  'bmessagerunner_single',
  [ 'bmessagerunner/SingleRunnerMain.cpp',
    'bmessagerunner/MessageRunnerTestHelpers.cpp',
    'bmessagerunner/BMessageRunnerTester.cpp' ],
  include_directories: [common_inc, cppunit_inc],
  dependencies: [dep_libroot, dep_betest],
  link_with: [lib_libbe],
  install: false
)

 # bmessagequeue single executable: not present in tree; keep tests in the shared library.