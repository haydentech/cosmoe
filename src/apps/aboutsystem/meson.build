srcs_aboutsystem = [
    'AboutSystem.cpp',
    'HyperTextActions.cpp',
    'HyperTextView.cpp',
    'Utilities.cpp',
]


aboutsystem_app = executable(
    'aboutsystem',
    srcs_aboutsystem,
    dependencies: dep_libbe,
    include_directories: common_inc,
    install: true,
)

srcs_aboutsystem_resources = [
    'AboutSystem.rdef'
]

aboutsystem_resources = custom_target('aboutsystem resources',
    depends : aboutsystem_app,
    input : srcs_aboutsystem_resources,
    output : 'AboutSystem.rsrc',
    command : ['rc', '@INPUT@', '-o', '@OUTPUT@', '-I', '.'],
    build_by_default : true)

custom_target('aboutsystem plus resources',
    depends : aboutsystem_resources,
    input : aboutsystem_app,
    output : 'fake',
    command : ['xres', '-o', '@INPUT@', aboutsystem_resources],
    build_by_default : true)