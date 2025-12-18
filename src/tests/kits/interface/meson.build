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

shared_library(
  'interfacekittest',
  srcs_interface_test,
  include_directories: [common_inc, cppunit_inc],
  dependencies: [dep_libroot, dep_betest],
  link_with: [lib_libbe],
  name_prefix: '',  # Don't add 'lib' prefix
  install: false
)
