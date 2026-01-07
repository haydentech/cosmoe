app_name = 'Clock'

app_srcs = [
    'cl_view.cpp',
    'cl_wind.cpp',
    'clock.cpp',
]

app_resource_defs = [
    app_name + '.rdef'
]

# Compile .rdef to .rsrc
app_resources = custom_target('app resources',
    depends : [rc_app],
    input : app_resource_defs,
    output : app_name + '.rsrc',
    command : [rc_app, '@INPUT@', '-o', '@OUTPUT@', '-I', '.'],
    build_by_default : true)

# Create a .o file with the resources embedded as a section
if is_darwin
    # macOS: Strip the first 4 bytes (RS magic) from .rsrc file
    # The embedded format doesn't need the file magic, just the resource data
    app_resources_stripped = custom_target('app resources stripped',
        input : app_resources,
        output : app_name + '_rsrc_data',
        command : ['dd', 'if=@INPUT@', 'of=@OUTPUT@', 'bs=4', 'skip=1', 'status=none'],
        build_by_default : true)
    
    # Step 1: Create empty C file and compile it
    app_resources_stub = custom_target('app resources stub',
        output : app_name + '_stub.o',
        command : [
            'sh', '-c',
            'echo "void _cosmoe_rsrc_stub_@0@(void) {}" | '.format(app_name) +
            cc.cmd_array()[0] + ' -x c - -c -o @OUTPUT@'
        ],
        build_by_default : true)
    
    # Step 2: Use ld -r to add the resource section (without RS magic) to the stub object
    app_resources_obj = custom_target(app_name + ' resources object',
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
    # Linux: Use ld -r -b binary to create a .o file
    # Add .note.GNU-stack section to prevent executable stack warning
    app_resources_obj = custom_target(app_name + ' resources object',
        input : app_resources,
        output : app_name + '_rsrc.o',
        command : [
            'sh', '-c',
            # Create object with embedded binary (use basename so symbols don't include path)
            'INPUT_DIR=$(dirname "$1") && INPUT_FILE=$(basename "$1") && OUTPUT_ABS=$(cd $(dirname "$2") && pwd)/$(basename "$2") && ' +
            'cd "$INPUT_DIR" && ld -r -b binary "$INPUT_FILE" -o "$OUTPUT_ABS" && ' +
            # Add .note.GNU-stack section to mark stack as non-executable
            'objcopy --add-section .note.GNU-stack=/dev/null ' +
            '--set-section-flags .note.GNU-stack=noload,readonly ' +
            '--redefine-sym _binary_' + app_name + '_rsrc_start=_binary_app_rsrc_start ' +
            '--redefine-sym _binary_' + app_name + '_rsrc_end=_binary_app_rsrc_end ' +
            '--redefine-sym _binary_' + app_name + '_rsrc_size=_binary_app_rsrc_size ' +
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
            '--redefine-sym _binary_' + app_name + '_rsrc_start=_binary_app_rsrc_start ' +
            '--redefine-sym _binary_' + app_name + '_rsrc_end=_binary_app_rsrc_end ' +
            '--redefine-sym _binary_' + app_name + '_rsrc_size=_binary_app_rsrc_size ' +
            '"$1" "$2"',
            '--', '@INPUT@', '@OUTPUT@'
        ],
        build_by_default : true)
endif

be_app = executable(
    app_name,
    app_srcs,
    objects: [app_resources_obj],
    dependencies: dep_libbe,
    include_directories: common_inc,
    install: true,
    link_args: is_windows ? ['-Wl,--disable-stdcall-fixup', '-Wl,--enable-auto-import', '-Wl,--no-insert-timestamp', '-static-libgcc', '-static-libstdc++', '-lmsvcrt'] : [],
    win_subsystem: is_windows ? 'windows' : 'console',
)

# macOS: Create .icns file from VICN resource and bundle the app
if is_darwin
    app_icns = custom_target('app icns',
        input : app_resources,
        output : app_name + '.icns',
        depends : [vicn2icns_prog],
        command : [vicn2icns_prog, '@INPUT@', '@OUTPUT@'],
        build_by_default : true)
    
    meson.add_install_script('sh', '-c', '''
        APP_NAME="''' + app_name + '''"
        BUNDLE_DIR="$DESTDIR''' + get_option('prefix') + '''/Applications/${APP_NAME}.app"
        mkdir -p "${BUNDLE_DIR}/Contents/MacOS"
        mkdir -p "${BUNDLE_DIR}/Contents/Resources"
        mv "$DESTDIR''' + get_option('prefix') + '''/bin/${APP_NAME}" "${BUNDLE_DIR}/Contents/MacOS/"
        cp "''' + meson.current_build_dir() + '''/${APP_NAME}.icns" "${BUNDLE_DIR}/Contents/Resources/"
        cat > "${BUNDLE_DIR}/Contents/Info.plist" << EOF
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
EOF
    ''')
endif