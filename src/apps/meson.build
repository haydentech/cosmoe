linux_desktop_entry_install_script = meson.project_source_root() + '/src/apps/desktop/install_linux_desktop_entry.sh'
mimeset_install_script = meson.project_source_root() + '/src/apps/install_with_mimeset.sh'

common_app_link_args = is_windows ? [
    '-Wl,--disable-stdcall-fixup',
    '-Wl,--enable-auto-import',
    '-Wl,--no-insert-timestamp',
    '-static-libgcc',
    '-static-libstdc++',
    '-lmsvcrt',
] : []

cosmoe_app_resource_artwork = custom_target('cosmoe app resource artwork',
    input : [meson.global_source_root() + '/data/artwork/cosmoe-logo.png'],
    output : ['res'],
    command : ['cp', '-r', meson.global_source_root() + '/data/artwork', meson.global_build_root() + '/data/'],
    install : false,
    build_by_default : true)

app_dirs = [
    'aboutsystem',
	'chart',
    'clock',
    'charactermap',
    'columns',
    'deskbar',
    'deskcalc',
    'diskusage',
    'fontdemo',
    'gradients',
    'showcase',
    'gui-torture',
    'icon-o-matic',
    'mandelbrot',
    'minimal',
    'minimal-noui',
    'pairs',
    'pulse',
    'resedit',
    'showimage',
    'stylededit',
    'sudoku',
    'terminal',
	'webpositive'
]

if is_linux
    app_dirs += ['tracker']
endif

foreach app_dir : app_dirs
    app_name = ''
    app_enabled = true
    app_srcs = files()
    app_resource_defs = files()
    app_resource_include_dirs = []
    app_resource_depends = []
    app_resource_depend_files = []
    app_uses_artwork_resources = false
    app_cpp_args = []
    app_link_with = []
    app_dependencies = []
    app_include_directories = []
    app_link_args = common_app_link_args
    app_win_subsystem = is_windows ? 'windows' : 'console'
    app_target_name = ''
    linux_desktop_id = ''
    linux_desktop_categories = ''

    subdir(app_dir)

    if not app_enabled
        continue
    endif

    if app_resource_defs.length() > 0
        app_resource_include_dirs += [meson.current_source_dir() / app_dir]
        if app_uses_artwork_resources
            app_resource_depends += [cosmoe_app_resource_artwork]
            app_resource_include_dirs += [meson.global_build_root() + '/data/artwork']
            app_resource_depend_files += [meson.global_source_root() + '/data/artwork/cosmoe-logo.png']
        endif

        app_resource_args = []
        foreach resource_include_dir : app_resource_include_dirs
            app_resource_args += ['-I', resource_include_dir]
        endforeach

        app_resources = custom_target(app_name + ' resources',
            depends : rc_app_dep + app_resource_depends,
            depend_files : app_resource_depend_files,
            input : app_resource_defs,
            output : app_name + '.rsrc',
            command : [rc_app, '@INPUT@', '-o', '@OUTPUT@'] + app_resource_args,
            build_by_default : true)

        if is_darwin
            app_resources_stub = custom_target(app_name + ' resources stub',
                output : app_name + '_stub.o',
                command : [
                    'sh', '-c',
                    'echo "void _cosmoe_rsrc_stub_@0@(void) {}" | '.format(app_name.replace('-', '_')) +
                    ' '.join(cc.cmd_array()) + ' -x c - -c -o @OUTPUT@'
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

            app_ico = custom_target(app_name + ' ico',
                input : app_resources,
                output : app_name + '.ico',
                depends : [vicn2ico_prog],
                command : [vicn2ico_prog, '@INPUT@', '@OUTPUT@'],
                build_by_default : true)

            app_rc = custom_target(app_name + ' rc',
                output : app_name + '.rc',
                input : app_ico,
                capture : true,
                command : ['echo', 'IDI_ICON1 ICON "' + app_name + '.ico"'],
                build_by_default : true)

            app_icon_obj = custom_target(app_name + ' icon object',
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

        app_objects = is_windows ? [app_resources_obj, app_icon_obj] : [app_resources_obj]
    else
        app_objects = []
    endif

    app_include_directories += include_directories(app_dir)
    if app_target_name == ''
        app_target_name = app_name == app_dir ? app_name + '-app' : app_name
    endif
    app_install_with_meson = app_target_name == app_name

    app_executable = executable(
        app_target_name,
        app_srcs,
        objects : app_objects,
        cpp_args : app_cpp_args,
        link_with : app_link_with,
        dependencies : app_dependencies,
        include_directories : app_include_directories,
        install : app_install_with_meson,
        link_args : app_link_args,
        win_subsystem : app_win_subsystem,
    )

    if not app_install_with_meson
        meson.add_install_script(
            'sh', '-c',
            'mkdir -p "$DESTDIR$1/bin" && install -m 755 "$2" "$DESTDIR$1/bin/$3"',
            'install-colliding-app',
            get_option('prefix'),
            app_executable.full_path(),
            app_name)
    endif

    if is_linux and linux_desktop_id != ''
        meson.add_install_script(
            'sh', linux_desktop_entry_install_script,
            get_option('prefix'),
            linux_desktop_id,
            app_name,
            app_name,
            app_name.to_lower(),
            linux_desktop_categories)
    endif

    if is_linux
        meson.add_install_script(
            'sh', mimeset_install_script,
            meson.global_build_root() / 'mimeset',
            meson.global_build_root(),
            dir_bin,
            dir_cosmoe_data / 'mime_db',
            app_name)
    endif

    if is_darwin
        # The app is installed as a .app bundle on macOS, with every non-Apple
        # dylib copied in so it runs without Homebrew installed.
        app_icns = ''
        if app_resource_defs.length() > 0
            custom_target(app_name + ' icns',
                input : app_resources,
                output : app_name + '.icns',
                depends : [vicn2icns_prog],
                command : [vicn2icns_prog, '@INPUT@', '@OUTPUT@'],
                build_by_default : true)

            app_icns = meson.current_build_dir() / (app_name + '.icns')
        endif

        meson.add_install_script(
            'sh', macos_app_bundle_script,
            app_name,
            get_option('prefix'),
            app_icns,
            version_cosmoe)
    elif is_linux and app_resource_defs.length() > 0
        meson.add_install_script('sh', '-c', '''
                RSRC_FILE="''' + meson.current_build_dir() + '/' + app_name + '''.rsrc"
                ICON_DIR="$DESTDIR''' + dir_data + '''/icons"
                APP_NAME_LOWER="''' + app_name.to_lower() + '''"

                "''' + vicn2png_prog.full_path() + '''" \
                    "${RSRC_FILE}" \
                    "${ICON_DIR}" \
                    "${APP_NAME_LOWER}"

                if command -v gtk-update-icon-cache >/dev/null 2>&1; then
                    gtk-update-icon-cache -f -t "${ICON_DIR}/hicolor" 2>/dev/null || true
                fi
            ''')
    endif
endforeach

deskbar_applications_menu = [
    'CharacterMap',
    'DeskCalc',
    'Icon-O-Matic',
    'ShowImage',
    'StyledEdit',
    'Terminal',
]

deskbar_demos_menu = [
    'Showcase',
    'Clock',
	'Chart',
    'FontDemo',
    'Gradients',
    'Mandelbrot',
    'Pairs',
    'Pulse',
    'Sudoku',
]

deskbar_preferences_menu = [
    'Appearance',
	'Backgrounds',
    'DeskbarPreferences',
	'Keymap',
    'Time',
	'TrackerPreferences',
]

# Ensure Deskbar's system menu source exists for SetNavDir fallback.
deskbar_menu_entries_dir = dir_cosmoe_data / 'deskbar'

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

foreach item : deskbar_preferences_menu
    symlink_name = item.replace('Preferences', '')
    install_symlink(symlink_name,
        pointing_to : (dir_prefix / get_option('bindir') / item),
        install_dir : deskbar_menu_entries_dir / 'menu' / 'Preferences')
endforeach

deskbar_menu_entries_conf = configuration_data()
deskbar_menu_entries_conf.set('DESKBAR_MENU_DIRECTORY',
    deskbar_menu_entries_dir / 'menu')

deskbar_menu_entries = configure_file(
    input : 'deskbar/menu_entries',
    output : 'menu_entries',
    configuration : deskbar_menu_entries_conf,
)

install_data(deskbar_menu_entries, install_dir : deskbar_menu_entries_dir)
