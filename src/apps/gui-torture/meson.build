app_name = 'gui-torture'

srcs_gui_torture = [
    'app.cpp',
    'disapp.cpp',
    'diswindow.cpp',
    'disview.cpp'
]

srcs_gui_torture_resources = [
    app_name + '.rdef'
]

# Compile .rdef to .rsrc
gui_torture_resources = custom_target('gui-torture resources',
    depends : [rc_app],
    input : srcs_gui_torture_resources,
    output : app_name + '.rsrc',
    command : [rc_app, '@INPUT@', '-o', '@OUTPUT@', '-I', '.'],
    build_by_default : true)

# Create a .o file with the resources embedded as a section
if is_darwin
    # macOS: Strip the first 4 bytes from .rsrc file
    gui_torture_resources_stripped = custom_target('gui-torture resources stripped',
        input : gui_torture_resources,
        output : app_name + '_rsrc_data',
        command : ['dd', 'if=@INPUT@', 'of=@OUTPUT@', 'bs=4', 'skip=1', 'status=none'],
        build_by_default : true)
    
    gui_torture_resources_stub = custom_target('gui-torture resources stub',
        output : app_name + '_stub.o',
        command : [
            'sh', '-c',
            'echo "void _cosmoe_rsrc_stub_' + app_name.underscorify() + '(void) {}" | ' +
            cc.cmd_array()[0] + ' -x c - -c -o @OUTPUT@'
        ],
        build_by_default : true)
    
    gui_torture_resources_obj = custom_target('gui-torture resources object',
        input : gui_torture_resources_stripped,
        output : app_name + '_rsrc.o',
        depends : gui_torture_resources_stub,
        command : [
            'ld', '-r',
            gui_torture_resources_stub,
            '-sectcreate', '__TEXT', '__rsrc', '@INPUT@',
            '-o', '@OUTPUT@'
        ],
        build_by_default : true)
elif is_linux
    gui_torture_resources_obj = custom_target('gui-torture resources object',
        input : gui_torture_resources,
        output : app_name + '_rsrc.o',
        command : [
            'ld', '-r', '-b', 'binary',
            '@INPUT@',
            '-o', '@OUTPUT@'
        ],
        build_by_default : true)
else
    gui_torture_resources_obj = []
endif

gui_torture_app = executable(
    app_name,
    srcs_gui_torture,
    objects: is_darwin or is_linux ? [gui_torture_resources_obj] : [],
    link_with: [
        lib_libbe,
        lib_libtracker ],
    dependencies: [
        dep_libbe,
        dep_libtracker
    ],
    include_directories: common_inc,
    install: true,
    link_args: is_windows ? ['-Wl,--disable-stdcall-fixup', '-Wl,--enable-auto-import', '-Wl,--no-insert-timestamp', '-static-libgcc', '-static-libstdc++', '-lmsvcrt'] : [],
    win_subsystem: is_windows ? 'windows' : 'console',
)