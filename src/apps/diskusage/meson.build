app_name = 'DiskUsage'

app_srcs = [
	'App.cpp',
	'ControlsView.cpp',
	'DiskUsage.cpp',
	'InfoWindow.cpp',
	'MainWindow.cpp',
	'PieView.cpp',
	'Scanner.cpp',
	'Snapshot.cpp',
	'StatusView.cpp',
	'VolumeView.cpp'
]


be_app = executable(
    app_name,
    app_srcs,
    objects: is_darwin or is_linux ? [app_resources_obj] : [],
    dependencies: dep_libbe,
    include_directories: [common_inc, tracker_inc],
    install: true,
    link_args: is_windows ? ['-Wl,--disable-stdcall-fixup', '-Wl,--enable-auto-import', '-Wl,--no-insert-timestamp', '-static-libgcc', '-static-libstdc++', '-lmsvcrt'] : [],
    win_subsystem: is_windows ? 'windows' : 'console',
)

app_resource_defs = [
    app_name + '.rdef'
]

app_resources = custom_target('app resources',
    depends : [rc_app],
    input : app_resource_defs,
    output : app_name + '.rsrc',
    command : [rc_app, '@INPUT@', '-o', '@OUTPUT@', '-I', '.'],
    build_by_default : true)

# Create a .o file with the resources embedded as a section
if is_darwin
    # macOS: Strip the first 4 bytes from .rsrc file
    app_resources_stripped = custom_target('app resources stripped',
        input : app_resources,
        output : app_name + '_rsrc_data',
        command : ['dd', 'if=@INPUT@', 'of=@OUTPUT@', 'bs=4', 'skip=1', 'status=none'],
        build_by_default : true)
    
    app_resources_stub = custom_target('app resources stub',
        output : app_name + '_stub.o',
        command : [
            'sh', '-c',
            'echo "void _cosmoe_rsrc_stub_@0@(void) {}" | '.format(app_name) +
            cc.cmd_array()[0] + ' -x c - -c -o @OUTPUT@'
        ],
        build_by_default : true)
    
    app_resources_obj = custom_target('app resources object',
        input : app_resources_stripped,
        output : app_name + '_rsrc.o',
        depends : app_resources_stub,
        command : [
            'ld', '-r',
            app_resources_stub,
            '-sectcreate', '__TEXT', '__rsrc', '@INPUT@',
            '-o', '@OUTPUT@'
        ],
        build_by_default : true)
elif is_linux
    app_resources_obj = custom_target('app resources object',
        input : app_resources,
        output : app_name + '_rsrc.o',
        command : [
            'ld', '-r', '-b', 'binary',
            '@INPUT@',
            '-o', '@OUTPUT@'
        ],
        build_by_default : true)
else
    app_resources_obj = []
endif

