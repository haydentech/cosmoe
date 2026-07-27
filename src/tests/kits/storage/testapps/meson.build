storage_testapp_deps = [dep_libbe]
storage_testapp_link_with = [lib_libbe, lib_localestub]

storage_testapp_targets = []

foreach testapp : [
  ['PathMonitorTest', 'PathMonitorTest.cpp'],
  ['PathMonitorTest2', 'PathMonitorTest2.cpp'],
  ['NodeMonitorTest', 'NodeMonitorTest.cpp'],
  ['dump_mime_types', 'dump_mime_types.cpp'],
  ['clipboard', 'clipboard.cpp'],
]
  storage_testapp_targets += executable(
    testapp[0],
    testapp[1],
    include_directories: common_inc,
    dependencies: storage_testapp_deps,
    link_with: storage_testapp_link_with,
    install: false,
  )
endforeach

alias_target('storage_testapps', storage_testapp_targets)
