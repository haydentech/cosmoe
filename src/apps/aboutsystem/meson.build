srcs_aboutsystem = [
    'AboutSystem.cpp',
    'HyperTextActions.cpp',
    'HyperTextView.cpp',
    'Utilities.cpp',
]

cosmoe_version = run_command('date', '+%F', check: true).stdout().strip()

aboutsystem_app = executable(
    'AboutSystem',
    srcs_aboutsystem,
    cpp_args: ['-DCOSMOE_VERSION=(built ' + cosmoe_version + ')'],
    dependencies: dep_libbe,
    include_directories: common_inc,
    install: true,
    link_args: is_windows ? ['-Wl,--disable-stdcall-fixup', '-Wl,--enable-auto-import', '-Wl,--no-insert-timestamp', '-static-libgcc', '-static-libstdc++', '-lmsvcrt'] : [],
    gui_app: is_windows,
)

srcs_aboutsystem_resources = [
    'AboutSystem.rdef',
]

# Move the artwork to the build directory so the resource compiler can find it
resource_artwork = custom_target('resource_artwork',
    input: [meson.global_source_root() + '/data/artwork/Walter logo.png'],
    output: ['res'],
    command: ['cp', '-r', meson.global_source_root() + '/data/artwork', meson.global_build_root() + '/data/'],
    install: false,
    build_by_default: true
)

aboutsystem_resources = custom_target('aboutsystem resources',
    depends : [aboutsystem_app, resource_artwork],
    depend_files : meson.global_source_root() + '/data/artwork/Walter logo.png',
    input : srcs_aboutsystem_resources,
    output : 'AboutSystem.rsrc',
    command : [rc_app, '@INPUT@', '-o', '@OUTPUT@', '-I', '.', '-I', meson.global_build_root() + '/data/artwork'],
    build_by_default : true)

custom_target('aboutsystem plus resources',
    depends : aboutsystem_resources,
    input : aboutsystem_app,
    output : 'fake',
    command : [xres_app, '-o', '@INPUT@', aboutsystem_resources],
    build_by_default : true)