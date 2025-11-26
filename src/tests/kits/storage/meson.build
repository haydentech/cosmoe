# Storage Kit Test Addon

srcs_storage_test = files(
  # 'AppFileInfoTest.cpp',  # TODO: BMimeType::IsInstalled not implemented
  'BasicTest.cpp',
  # 'DataIOTest.cpp',       # TODO: BBufferedDataIO not implemented
  'DirectoryTest.cpp',
  'EntryTest.cpp',
  'FileTest.cpp',
  'FindDirectoryTest.cpp',
  # 'MimeSnifferTest.cpp',  # TODO: Enable when ready
  # 'MimeTypeTest.cpp',     # TODO: Enable when ready
  # 'NodeInfoTest.cpp',     # TODO: BNodeInfo::GetPreferredApp/SetPreferredApp not implemented
  'NodeTest.cpp',
  'PathTest.cpp',
  # 'QueryTest.cpp',        # TODO: Hangs on destruction
  # 'ResourcesTest.cpp',    # TODO: Enable when ready
  # 'ResourceStringsTest.cpp',  # TODO: Enable when ready
  'StatableTest.cpp',
  'StorageKitTestAddon.cpp',
  'SymLinkTest.cpp',
  'TestApp.cpp',
  # 'VolumeTest.cpp',       # TODO: mkbfs missing
)

shared_library(
  'storagekittest',
  srcs_storage_test,
  include_directories: [common_inc, cppunit_inc],
  dependencies: [dep_libroot, dep_betest],
  link_with: [lib_libbe],
  name_prefix: '',  # Don't add 'lib' prefix
  install: true,
  install_dir: get_option('libdir') / 'unittester' / 'add-ons'
)
