app_name = 'Deskbar'

app_srcs = [
    'BarApp.cpp',
    'BarMenuBar.cpp',
    'BarMenuTitle.cpp',
    'BarView.cpp',
    'BarWindow.cpp',
    'DeskbarMenu.cpp',
    'DeskbarUtils.cpp',
    'ExpandoMenuBar.cpp',
    'InlineScrollView.cpp',
    'PreferencesWindow.cpp',
    'ShowHideMenuItem.cpp',
    'StatusView.cpp',
    'StatusViewShelf.cpp',
    'TeamMenu.cpp',
    'TeamMenuItem.cpp',
    'TimeView.cpp',
    'TruncatableMenuItem.cpp',
    'WindowMenu.cpp',
    'WindowMenuItem.cpp',
    'ResourceSet.cpp',
    'Switcher.cpp',
    'CalendarMenuWindow.cpp',
	'ScreenCornerSelector.cpp',
]

app_resource_defs = [
    'Deskbar.rdef',
    'icons.rdef',
]

# Compile .rdef to .rsrc
app_resources = custom_target('deskbar resources',
    depends : rc_app_dep,
    input : app_resource_defs,
    output : app_name + '.rsrc',
    command : [
        rc_app,
        '@INPUT@',
        '-o', '@OUTPUT@',
        '-I', meson.current_source_dir(),
        '-I', meson.current_build_dir(),
        '-I', '.',
    ],
    build_by_default : true)

# Create a .o file with the resources embedded as a section
if is_darwin
    app_resources_stub = custom_target('deskbar resources stub',
        output : app_name + '_stub.o',
        command : [
            'sh', '-c',
            'echo "void _cosmoe_rsrc_stub_@0@(void) {}" | '.format(app_name) +
            cc.cmd_array()[0] + ' -x c - -c -o @OUTPUT@'
        ],
        build_by_default : true)

    app_resources_obj = custom_target(app_name + ' resources object',
        input : app_resources,
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
    app_resources_obj = custom_target(app_name + ' resources object',
        input : app_resources,
        output : app_name + '_rsrc.o',
        command : [
            'sh', '-c',
            'INPUT_DIR=$(dirname "$1") && INPUT_FILE=$(basename "$1") && OUTPUT_ABS=$(cd $(dirname "$2") && pwd)/$(basename "$2") && ' +
            'cd "$INPUT_DIR" && ld -r -b binary "$INPUT_FILE" -o "$OUTPUT_ABS" && ' +
            'objcopy --rename-section .data=.cosmoe_rsrc "$OUTPUT_ABS" && ' +
            'objcopy --add-section .note.GNU-stack=/dev/null ' +
            '--set-section-flags .note.GNU-stack=noload,readonly ' +
            '"$OUTPUT_ABS"',
            '--', '@INPUT@', '@OUTPUT@'
        ],
        build_by_default : true)
else
    app_resources_obj = custom_target(app_name + ' resources object',
        input : app_resources,
        output : app_name + '_rsrc.o',
        command : [
            'sh', '-c',
            'objcopy -I binary -O pe-x86-64 -B i386:x86-64 ' +
            '--rename-section .data=.cosmoe_rsrc ' +
            '"$1" "$2"',
            '--', '@INPUT@', '@OUTPUT@'
        ],
        build_by_default : true)

    app_ico = custom_target('deskbar ico',
        input : app_resources,
        output : app_name + '.ico',
        depends : [vicn2ico_prog],
        command : [vicn2ico_prog, '@INPUT@', '@OUTPUT@'],
        build_by_default : true)

    app_rc = custom_target('deskbar rc',
        output : app_name + '.rc',
        input : app_ico,
        capture : true,
        command : ['echo', 'IDI_ICON1 ICON "' + app_name + '.ico"'],
        build_by_default : true)

    app_icon_obj = custom_target('deskbar icon object',
        input : app_rc,
        output : app_name + '_icon.o',
        depends : [app_ico],
        command : [
            'sh', '-c',
            'cd "$(dirname "$1")" && "' + windres.full_path() + '" "$(basename "$1")" -o "$(basename "$2")"',
            '--', '@INPUT@', '@OUTPUT@'
        ],
        build_by_default : true)
endif

be_app = executable(
    app_name,
    app_srcs,
    objects: is_windows ? [app_resources_obj, app_icon_obj] : [app_resources_obj],
    dependencies: [dep_libbe, dep_libtracker],
    include_directories: [common_inc, tracker_inc],
    cpp_args: [
        '-DDB_ADDONS',
        '-D_INCLUDES_CLASS_DEVICE_MAP=1',
        '-D_SUPPORTS_RESOURCES=1',
        '-D_SUPPORTS_FEATURE_SCRIPTING=1',
        '-D_SHOW_CALENDAR_MENU_WINDOW=1',
        '-DMOUNT_MENU_IN_DESKBAR=1',
    ],
    install: true,
    link_args: is_windows ? ['-Wl,--disable-stdcall-fixup', '-Wl,--enable-auto-import', '-Wl,--no-insert-timestamp', '-static-libgcc', '-static-libstdc++', '-lmsvcrt'] : [],
    win_subsystem: is_windows ? 'windows' : 'console',
)


deskbar_applications_menu = [
	'Icon-O-Matic',
	'Terminal'
]

deskbar_demos_menu = [
	'Showcase',
	'Clock',
	'FontDemo',
	'Mandelbrot',
	'Pairs',
	'Pulse',
	'Sudoku',
] 

# Ensure Deskbar's system menu source exists for SetNavDir fallback.
deskbar_menu_entries_dir = dir_prefix / get_option('sysconfdir') / 'cosmoe' / 'deskbar'

foreach item : deskbar_applications_menu
    install_symlink(item,
        pointing_to : (dir_prefix / get_option('bindir') / item),
        install_dir : deskbar_menu_entries_dir / 'menu' / 'Applications')
endforeach

foreach item : deskbar_demos_menu
    install_symlink(item,
        pointing_to : (dir_prefix / get_option('bindir') / item),
        install_dir : deskbar_menu_entries_dir / 'menu' / 'Demos')
endforeach

install_data('menu_entries', install_dir : deskbar_menu_entries_dir)
