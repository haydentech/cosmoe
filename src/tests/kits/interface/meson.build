subdir('picture')

# Interface Kit Test Addon

srcs_interface_test = files(
  'InterfaceKitTestAddon.cpp',
  'GraphicsDefsTest.cpp',
  'OutlineListViewTest.cpp',
  # bpolygon tests
  'bpolygon/PolygonTest.cpp',
  'bpolygon/CreatePolygonTest.cpp',
  'bpolygon/MapPolygonTest.cpp',
  'bpolygon/DummyPolygon.cpp',
  # bregion tests
  'bregion/RegionTest.cpp',
  'bregion/RegionConstruction.cpp',
  'bregion/RegionExclude.cpp',
  'bregion/RegionInclude.cpp',
  'bregion/RegionIntersect.cpp',
  'bregion/RegionOffsetBy.cpp',
  'bregion/RegionTestcase.cpp',
  # btextcontrol tests
  'btextcontrol/TextControlTest.cpp',
  # btextview tests
  'btextview/TextViewTest.cpp',
)

# Add Windows DLL entry point stub on Windows
if is_windows
  srcs_interface_test += files('../test_addon_stub.cpp')
endif

shared_library(
  'interfacekittest',
  srcs_interface_test,
  include_directories: [common_inc, cppunit_inc],
  dependencies: [dep_libroot, dep_betest],
  link_with: [lib_libbe],
  name_prefix: '',  # Don't add 'lib' prefix
  install: false
)

# Standalone test programs
executable(
  'StatusBarTest',
  'StatusBarTest.cpp',
  include_directories: common_inc,
  dependencies: dep_libroot,
  link_with: lib_libbe,
  install: false
)

executable(
  'SliderTest',
  'SliderTest.cpp',
  include_directories: common_inc,
  dependencies: dep_libroot,
  link_with: lib_libbe,
  install: false
)

executable(
  'ListViewTest',
  'ListViewTest.cpp',
  include_directories: common_inc,
  dependencies: dep_libroot,
  link_with: lib_libbe,
  install: false
)

executable(
  'SimpleAlertTest',
  'SimpleAlertTest.cpp',
  include_directories: common_inc,
  dependencies: dep_libroot,
  link_with: lib_libbe,
  install: false
)

executable(
  'TruncateString',
  'TruncateString.cpp',
  include_directories: common_inc,
  dependencies: dep_libroot,
  link_with: lib_libbe,
  install: false
)

executable(
  'WidthBufferTest',
  'WidthBufferTest.cpp',
  include_directories: common_inc,
  dependencies: dep_libroot,
  link_with: lib_libbe,
  install: false
)

executable(
  'ToolTipTest',
  'ToolTipTest.cpp',
  include_directories: common_inc,
  dependencies: dep_libroot,
  link_with: lib_libbe,
  install: false
)

executable(
  'ScrollViewTest',
  'ScrollViewTest.cpp',
  include_directories: common_inc,
  dependencies: dep_libroot,
  link_with: lib_libbe,
  install: false
)

executable(
  'ClippingPlusRedraw',
  'ClippingPlusRedraw.cpp',
  include_directories: common_inc,
  dependencies: dep_libroot,
  link_with: lib_libbe,
  install: false
)

executable(
  'CheckBoxTest',
  'CheckBoxTest.cpp',
  include_directories: common_inc,
  dependencies: dep_libroot,
  link_with: lib_libbe,
  install: false
)

executable(
  'ChannelSliderTest',
  'ChannelSliderTest.cpp',
  include_directories: common_inc,
  dependencies: dep_libroot,
  link_with: lib_libbe,
  install: false
)

executable(
  'PulseTest',
  'PulseTest.cpp',
  include_directories: common_inc,
  dependencies: dep_libroot,
  link_with: lib_libbe,
  install: false
)

executable(
  'ControlLookTest',
  'ControlLookTest.cpp',
  include_directories: common_inc,
  dependencies: dep_libroot,
  link_with: lib_libbe,
  install: false
)

executable(
  'TextViewTestManual',
  'TextViewTestManual.cpp',
  include_directories: common_inc,
  dependencies: dep_libroot,
  link_with: lib_libbe,
  install: false
)

executable(
  'MenuBeginningTest',
  'MenuBeginningTest.cpp',
  include_directories: common_inc,
  dependencies: dep_libroot,
  link_with: lib_libbe,
  install: false
)

executable(
  'MenuTriggerTest',
  'MenuTriggerTest.cpp',
  include_directories: common_inc,
  dependencies: dep_libroot,
  link_with: lib_libbe,
  install: false
)


