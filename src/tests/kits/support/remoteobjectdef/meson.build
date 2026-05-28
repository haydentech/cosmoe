# Support Kit remote object definition test library

remote_object_name = 'libsupporttest_RemoteTestObject'

srcs_remote_object = files(
  'RemoteTestObject.cpp',
)

srcs_remote_object_resources = files(
  'RemoteTestObject.rdef',
)

remote_object_resources = custom_target('remoteobjectdef resources',
  depends: rc_app_dep,
  input: srcs_remote_object_resources,
  output: 'RemoteTestObject.rsrc',
  command: [rc_app, '@INPUT@', '-o', '@OUTPUT@', '-I', '.'],
  build_by_default: true,
)

# Create an object with embedded resources section.
if is_darwin
  remote_object_resources_stub = custom_target('remoteobjectdef resources stub',
    output: 'remoteobjectdef_stub.o',
    command: [
      'sh', '-c',
      'echo "void _cosmoe_rsrc_stub_remoteobjectdef(void) {}" | '
      + ' '.join(cc.cmd_array()) + ' -x c - -c -o @OUTPUT@'
    ],
    build_by_default: true,
  )

  remote_object_resources_obj = custom_target('remoteobjectdef resources object',
    input: remote_object_resources,
    output: 'remoteobjectdef_rsrc.o',
    depends: remote_object_resources_stub,
    command: [
      'ld', '-r',
      remote_object_resources_stub,
      '-sectcreate', '__TEXT', '__rsrc', '@INPUT@',
      '-o', '@OUTPUT@'
    ],
    build_by_default: true,
  )
elif is_linux
  remote_object_resources_obj = custom_target('remoteobjectdef resources object',
    input: remote_object_resources,
    output: 'remoteobjectdef_rsrc.o',
    command: [
      'sh', '-c',
      'ld -r -b binary "$1" -o "$2" && '
      + 'objcopy --rename-section .data=.cosmoe_rsrc "$2" && '
      + 'objcopy --add-section .note.GNU-stack=/dev/null '
      + '--set-section-flags .note.GNU-stack=noload,readonly "$2"',
      '--', '@INPUT@', '@OUTPUT@'
    ],
    build_by_default: true,
  )
else
  remote_object_resources_obj = []
endif

shared_library(
  remote_object_name,
  srcs_remote_object,
  objects: is_darwin or is_linux ? [remote_object_resources_obj] : [],
  include_directories: common_inc,
  dependencies: dep_libroot,
  link_with: [lib_libbe],
  name_prefix: '',
  install: false,
)
