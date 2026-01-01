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
