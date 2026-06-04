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
    'minitracker',
    'msgtest',
    'pairs',
    'pulse',
    'resedit',
    'showimage',
    'stylededit',
    'sudoku',
    'terminal',
]

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
            app_name)
    endif

    if app_resource_defs.length() > 0
        if is_darwin
            custom_target(app_name + ' icns',
                input : app_resources,
                output : app_name + '.icns',
                depends : [vicn2icns_prog],
                command : [vicn2icns_prog, '@INPUT@', '@OUTPUT@'],
                build_by_default : true)

            meson.add_install_script('sh', '-c', '''
                APP_NAME="''' + app_name + '''"
                PREFIX="''' + get_option('prefix') + '''"
                BUNDLE_DIR="$DESTDIR${PREFIX}/Applications/${APP_NAME}.app"
                mkdir -p "${BUNDLE_DIR}/Contents/MacOS"
                mkdir -p "${BUNDLE_DIR}/Contents/Resources"
                mv "$DESTDIR${PREFIX}/bin/${APP_NAME}" "${BUNDLE_DIR}/Contents/MacOS/"
                cp "''' + meson.current_build_dir() + '''/${APP_NAME}.icns" "${BUNDLE_DIR}/Contents/Resources/"
                cat > "${BUNDLE_DIR}/Contents/Info.plist" << PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleExecutable</key>
    <string>${APP_NAME}</string>
    <key>CFBundleIconFile</key>
    <string>${APP_NAME}.icns</string>
    <key>CFBundleIdentifier</key>
    <string>org.cosmoe.${APP_NAME}</string>
    <key>CFBundleName</key>
    <string>${APP_NAME}</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleShortVersionString</key>
    <string>1.0</string>
    <key>CFBundleVersion</key>
    <string>1</string>
    <key>NSHighResolutionCapable</key>
    <true/>
</dict>
</plist>
PLIST
            ''')
        elif is_linux
            meson.add_install_script('sh', '-c', '''
                RSRC_FILE="''' + meson.current_build_dir() + '/' + app_name + '''.rsrc"
                ICON_DIR="$DESTDIR''' + get_option('prefix') + '''/share/icons"
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
    endif
endforeach
