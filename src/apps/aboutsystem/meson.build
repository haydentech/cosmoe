srcs_aboutsystem = [
    'AboutSystem.cpp',
    'HyperTextActions.cpp',
    'HyperTextView.cpp',
    'Utilities.cpp',
]

cosmoe_version = run_command('date', '+%F').stdout().strip()

aboutsystem_app = executable(
    'aboutsystem',
    srcs_aboutsystem,
    cpp_args: ['-DCOSMOE_VERSION=(built ' + cosmoe_version + ')'],
    dependencies: dep_libbe,
    include_directories: common_inc,
    install: true,
)

srcs_aboutsystem_resources = [
    'AboutSystem.rdef',
]

# Move the artwork to the build directory so the resource compiler can find it
resource_artwork = custom_target('resource_artwork',
    input: [meson.source_root() + '/data/artwork/Walter logo.png'],
    output: ['res'],
    command: ['cp', '-r', meson.source_root() + '/data/artwork', meson.build_root() + '/data/'],
    install: false,
    build_by_default: true
)

aboutsystem_resources = custom_target('aboutsystem resources',
    depends : aboutsystem_app,
    depend_files : meson.source_root() + '/data/artwork/Walter logo.png',
    input : srcs_aboutsystem_resources,
    output : 'AboutSystem.rsrc',
    command : [rc_app, '@INPUT@', '-o', '@OUTPUT@', '-I', '.', '-I', meson.build_root() + '/data/artwork'],
    build_by_default : true)

custom_target('aboutsystem plus resources',
    depends : aboutsystem_resources,
    input : aboutsystem_app,
    output : 'fake',
    command : [xres_app, '-o', '@INPUT@', aboutsystem_resources],
    build_by_default : true)