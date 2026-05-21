app_name = 'Appearance'
linux_desktop_id = 'x-vnd.Haiku-Appearance'
linux_desktop_categories = 'Settings;Utility;'

if is_linux
    meson.add_install_script(
        'sh', linux_desktop_entry_install_script,
        get_option('prefix'),
        linux_desktop_id,
        app_name,
        app_name,
        app_name.to_lower(),
        linux_desktop_categories)
endif

app_srcs = [
    'Appearance.cpp',
    'AppearanceWindow.cpp',
    'Colors.cpp',
    'ColorsView.cpp',
    'FakeScrollBar.cpp',
    'FontSelectionView.cpp',
    'FontView.cpp',
    'LookAndFeelSettingsView.cpp',
]

app_resource_defs = [
    app_name + '.rdef'
]

app_resources = custom_target('appearance resources',
    depends : rc_app_dep,
    input : app_resource_defs,
    output : app_name + '.rsrc',
    command : [rc_app, '@INPUT@', '-o', '@OUTPUT@', '-I', '.'],
    build_by_default : true)

if is_darwin
    app_resources_stub = custom_target('appearance resources stub',
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

    app_ico = custom_target('appearance ico',
        input : app_resources,
        output : app_name + '.ico',
        depends : [vicn2ico_prog],
        command : [vicn2ico_prog, '@INPUT@', '@OUTPUT@'],
        build_by_default : true)

    app_rc = custom_target('appearance rc',
        output : app_name + '.rc',
        input : app_ico,
        capture : true,
        command : ['echo', 'IDI_ICON1 ICON "' + app_name + '.ico"'],
        build_by_default : true)

    app_icon_obj = custom_target('appearance icon object',
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

appearance_app = executable(
    app_name,
    app_srcs,
    objects: is_windows ? [app_resources_obj, app_icon_obj] : [app_resources_obj],
    dependencies: [dep_libbe],
    include_directories: common_inc,
    install: true,
    link_args: is_windows ? ['-Wl,--disable-stdcall-fixup', '-Wl,--enable-auto-import', '-Wl,--no-insert-timestamp', '-static-libgcc', '-static-libstdc++', '-lmsvcrt'] : [],
    win_subsystem: is_windows ? 'windows' : 'console',
)

if is_linux
    meson.add_install_script(
        'sh', mimeset_install_script,
        meson.global_build_root() / 'mimeset',
        meson.global_build_root(),
        dir_bin,
        app_name)

    meson.add_install_script('sh', '-c', '''
        RSRC_FILE="''' + meson.current_build_dir() + '/' + app_name + '''.rsrc"
        ICON_DIR="$DESTDIR/usr/local/share/icons"
        APP_NAME_LOWER="appearance"

        "''' + meson.global_build_root() + '''/vicn2png" \
            "${RSRC_FILE}" \
            "${ICON_DIR}" \
            "${APP_NAME_LOWER}"

        if command -v gtk-update-icon-cache >/dev/null 2>&1; then
            gtk-update-icon-cache -f -t "${ICON_DIR}/hicolor" 2>/dev/null || true
        fi
    ''')
endif