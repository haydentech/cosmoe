app_name = 'StyledEdit'
mimeset_install_script = meson.project_source_root() + '/src/apps/install_with_mimeset.sh'
linux_desktop_id = 'x-vnd.Haiku-StyledEdit'
linux_desktop_categories = 'Utility;TextEditor;'
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
    'ColorMenuItem.cpp',
    'FindWindow.cpp',
    'ReplaceWindow.cpp',
    'StatusView.cpp',
    'StyledEditApp.cpp',
    'StyledEditView.cpp',
    'StyledEditWindow.cpp',
]

app_resource_defs = [
    app_name + '.rdef'
]

app_resources = custom_target('app resources',
    depends : rc_app_dep,
    input : app_resource_defs,
    output : app_name + '.rsrc',
    command : [rc_app, '@INPUT@', '-o', '@OUTPUT@', '-I', '.'],
    build_by_default : true)

# Create a .o file with the resources embedded as a section
if is_darwin
    # Step 1: Create empty C file and compile it
    app_resources_stub = custom_target('app resources stub',
        output : app_name + '_stub.o',
        command : [
            'sh', '-c',
            'echo "void _cosmoe_rsrc_stub_@0@(void) {}" | '.format(app_name) +
            ' '.join(cc.cmd_array()) + ' -x c - -c -o @OUTPUT@'
        ],
        build_by_default : true)

    # Step 2: Use ld -r to add the resource section to the stub object
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
    # Linux: Use ld -r -b binary to create a .o file, then rename section to .cosmoe_rsrc
    # Add .note.GNU-stack section to prevent executable stack warning
    app_resources_obj = custom_target(app_name + ' resources object',
        input : app_resources,
        output : app_name + '_rsrc.o',
        command : [
            'sh', '-c',
            # Create object with embedded binary (use basename so symbols don't include path)
            'INPUT_DIR=$(dirname "$1") && INPUT_FILE=$(basename "$1") && OUTPUT_ABS=$(cd $(dirname "$2") && pwd)/$(basename "$2") && ' +
            'cd "$INPUT_DIR" && ld -r -b binary "$INPUT_FILE" -o "$OUTPUT_ABS" && ' +
            # Rename .data section so it does not collide with executable .data and is discoverable at runtime
            'objcopy --rename-section .data=.cosmoe_rsrc "$OUTPUT_ABS" && ' +
            # Add .note.GNU-stack section to mark stack as non-executable
            'objcopy --add-section .note.GNU-stack=/dev/null ' +
            '--set-section-flags .note.GNU-stack=noload,readonly ' +
            '"$OUTPUT_ABS"',
            '--', '@INPUT@', '@OUTPUT@'
        ],
        build_by_default : true)
else
    # Windows: Use objcopy to embed resources in PE/COFF format
    # objcopy creates symbols based on filename, so we rename them to a standard name
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
    
    # Generate .ico file from VICN resource
    app_ico = custom_target('app ico',
        input : app_resources,
        output : app_name + '.ico',
        depends : [vicn2ico_prog],
        command : [vicn2ico_prog, '@INPUT@', '@OUTPUT@'],
        build_by_default : true)
    
    # Create .rc file that references the icon
    app_rc = custom_target('app rc',
        output : app_name + '.rc',
        input : app_ico,
        capture : true,
        command : ['echo', 'IDI_ICON1 ICON "' + app_name + '.ico"'],
        build_by_default : true)
    
    # Compile .rc to .res object
    app_icon_obj = custom_target('app icon object',
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
endif


# macOS app bundle support
if is_darwin
    stylededit_icns = custom_target('stylededit icns',
        input : app_resources,
        output : 'StyledEdit.icns',
        depends : [vicn2icns_prog],
        command : [vicn2icns_prog, '@INPUT@', '@OUTPUT@'],
        build_by_default : true)
    
    meson.add_install_script('sh', '-c', '''
        APP_NAME="StyledEdit"
        BUNDLE_DIR="$DESTDIR/usr/local/Applications/${APP_NAME}.app"
        mkdir -p "${BUNDLE_DIR}/Contents/MacOS"
        mkdir -p "${BUNDLE_DIR}/Contents/Resources"
        mv "$DESTDIR/usr/local/bin/${APP_NAME}" "${BUNDLE_DIR}/Contents/MacOS/"
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
    <string>com.cosmoe.${APP_NAME}</string>
    <key>CFBundleName</key>
    <string>${APP_NAME}</string>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>NSHighResolutionCapable</key>
    <true/>
</dict>
</plist>
PLIST
    ''')
endif

# Linux X11/Wayland icon installation
if is_linux
    meson.add_install_script('sh', '-c', '''
        RSRC_FILE="''' + meson.current_build_dir() + '/' + app_name + '''.rsrc"
        ICON_DIR="$DESTDIR/usr/local/share/icons"
        APP_NAME_LOWER="stylededit"
        
        # Run vicn2png to generate icons
        "''' + meson.global_build_root() + '''/vicn2png" \
            "${RSRC_FILE}" \
            "${ICON_DIR}" \
            "${APP_NAME_LOWER}"
        
        # Update icon cache if gtk-update-icon-cache is available
        if command -v gtk-update-icon-cache >/dev/null 2>&1; then
            gtk-update-icon-cache -f -t "${ICON_DIR}/hicolor" 2>/dev/null || true
        fi
    ''')
endif
