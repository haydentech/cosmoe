# Storage Kit Test Addon

srcs_storage_test = files(
  # 'AppFileInfoTest.cpp',  # TODO: BMimeType::IsInstalled not implemented
  'BasicTest.cpp',
  'DataIOTest.cpp',
  'DirectoryTest.cpp',
  'EntryTest.cpp',
  'FileTest.cpp',
  'FindDirectoryTest.cpp',
  # 'MimeSnifferTest.cpp',  # TODO: Enable when ready
  # 'MimeTypeTest.cpp',     # TODO: Enable when ready
  # 'NodeInfoTest.cpp',     # TODO: BNodeInfo::GetPreferredApp/SetPreferredApp not implemented
  'NodeTest.cpp',
  'PathTest.cpp',
  # 'QueryTest.cpp',        # TODO: Query unsupported on Cosmoe
  'ResourcesTest.cpp',
  'ResourceStringsTest.cpp',
  'StatableTest.cpp',
  'StorageKitTestAddon.cpp',
  'SymLinkTest.cpp',
  'TestApp.cpp',
  'VolumeTest.cpp',
)

storage_test_resources_src = meson.current_source_dir() + '/resources'
storage_test_resources_dirs = [
  meson.current_build_dir() + '/resources',
  meson.project_build_root() + '/src/tests/resources',
]

foreach resources_dir : storage_test_resources_dirs
  run_command(
    'sh',
    '-c',
    'mkdir -p "$1" && cp -R "$2"/. "$1"',
    'sh',
    resources_dir,
    storage_test_resources_src,
    check: true,
  )
endforeach

# Add Windows DLL entry point stub on Windows
if is_windows
  srcs_storage_test += files('../test_addon_stub.cpp')
endif

storage_test_deps = [dep_libroot, dep_betest]
storage_test_cpp_args = []

if host_machine.system() == 'linux'
  dep_fuse3 = dependency('fuse3', required: false)
  if dep_fuse3.found()
    storage_test_deps += dep_fuse3
    storage_test_cpp_args += ['-DCOSMOE_HAVE_FUSE3=1']
  endif
endif

shared_library(
  'storagekittest',
  srcs_storage_test,
  include_directories: [common_inc, cppunit_inc],
  dependencies: storage_test_deps,
  cpp_args: storage_test_cpp_args,
  link_with: [lib_libbe],
  name_prefix: '',  # Don't add 'lib' prefix
  install: false
)
