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
  'bmessenger/BMessengerTester.cpp',
  'bmessenger/MessengerTest.cpp',
  'bmessenger/MessengerAssignmentTester.cpp',
  'bmessenger/MessengerComparissonTester.cpp',
  'bmessenger/LockTargetTester.cpp',
  'bmessenger/LockTargetWithTimeoutTester.cpp',
  'bmessenger/SendMessageTester.cpp',
  'bmessenger/SMInvoker.cpp',
  'bmessenger/SMLooper.cpp',
  'bmessenger/SMReplyTarget.cpp',
  'bmessenger/SMTarget.cpp',
  'bmessenger/TargetTester.cpp',
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

  'common/AppRunner.cpp',
  'common/PipedAppRunner.cpp',

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
  include_directories: [common_inc, cppunit_inc, 'common'],
  dependencies: [dep_libroot, dep_betest],
  link_with: [lib_libbe],
  name_prefix: '',  # Don't add 'lib' prefix
  install: false
)

bmessagerunner_single_exe = executable(
  'bmessagerunner_single',
  [ 'bmessagerunner/SingleRunnerMain.cpp',
    'bmessagerunner/MessageRunnerTestHelpers.cpp',
    'bmessagerunner/BMessageRunnerTester.cpp' ],
  include_directories: [common_inc, cppunit_inc],
  dependencies: [dep_libroot, dep_betest],
  link_with: [lib_libbe],
  install: false
)

sm_remote_target_app_exe = executable(
  'SMRemoteTargetApp',
  [ 'bmessenger/SMRemoteTargetApp.cpp',
    'bmessenger/SMLooper.cpp' ],
  include_directories: [common_inc, cppunit_inc],
  dependencies: [dep_libroot, dep_betest],
  link_with: [lib_libbe],
  install: false
)

app_run_test_app1_exe = executable(
  'AppRunTestApp1',
  [ 'bmessenger/testapps/BMessengerTestApp1.cpp',
    'bmessenger/SMLooper.cpp',
	'common/CommonTestApp.cpp' ],
  include_directories: [common_inc, cppunit_inc, 'common'],
  dependencies: [dep_libroot, dep_betest],
  link_with: [lib_libbe],
  install: false
)

# Stage helper executables where BTestShell::GlobalTestDir() expects them.
_helper_staged_stamps = []
foreach helper : [
  ['bmessagerunner_single', bmessagerunner_single_exe],
  ['SMRemoteTargetApp', sm_remote_target_app_exe],
  ['AppRunTestApp1', app_run_test_app1_exe],
]
  _helper_staged_stamps += custom_target(
    'stage_' + helper[0] + '_into_src_tests',
    input: helper[1],
    output: 'stage_' + helper[0] + '.stamp',
    command: [
      'sh', '-c',
      'mkdir -p "$2" && tmp="$2/$3.tmp.$$" && cp "$1" "$tmp" && mv -f "$tmp" "$2/$3" && touch "$4"',
      'sh', '@INPUT@', meson.project_build_root() + '/src/tests', helper[0], '@OUTPUT@'
    ],
    build_by_default: true,
  )
endforeach

alias_target('stage_test_helper_apps', _helper_staged_stamps)

 # bmessagequeue single executable: not present in tree; keep tests in the shared library.