srcs_deskcalc = [
    'CalcApplication.cpp',
    'CalcOptions.cpp',
    'CalcView.cpp',
    'CalcWindow.cpp',
    'DeskCalc.cpp',
    'ExpressionTextView.cpp',
    'InputTextView.cpp',
]

srcs_deskcalc_resources = [
    'DeskCalc.rdef'
]

# Compile .rdef to .rsrc
deskcalc_resources = custom_target('deskcalc resources',
    depends : [rc_app],
    input : srcs_deskcalc_resources,
    output : 'DeskCalc.rsrc',
    command : [rc_app, '@INPUT@', '-o', '@OUTPUT@', '-I', '.'],
    build_by_default : true)

# Create a .o file with the resources embedded as a section
if is_darwin
    # macOS: Strip the first 4 bytes from .rsrc file
    deskcalc_resources_stripped = custom_target('deskcalc resources stripped',
        input : deskcalc_resources,
        output : 'DeskCalc_rsrc_data',
        command : ['dd', 'if=@INPUT@', 'of=@OUTPUT@', 'bs=4', 'skip=1', 'status=none'],
        build_by_default : true)
    
    deskcalc_resources_stub = custom_target('deskcalc resources stub',
        output : 'DeskCalc_stub.o',
        command : [
            'sh', '-c',
            'echo "void _cosmoe_rsrc_stub_DeskCalc(void) {}" | ' +
            cc.cmd_array()[0] + ' -x c - -c -o @OUTPUT@'
        ],
        build_by_default : true)
    
    deskcalc_resources_obj = custom_target('deskcalc resources object',
        input : deskcalc_resources_stripped,
        output : 'DeskCalc_rsrc.o',
        depends : deskcalc_resources_stub,
        command : [
            'ld', '-r',
            deskcalc_resources_stub,
            '-sectcreate', '__TEXT', '__rsrc', '@INPUT@',
            '-o', '@OUTPUT@'
        ],
        build_by_default : true)
elif is_linux
    deskcalc_resources_obj = custom_target('deskcalc resources object',
        input : deskcalc_resources,
        output : 'DeskCalc_rsrc.o',
        command : [
            'ld', '-r', '-b', 'binary',
            '@INPUT@',
            '-o', '@OUTPUT@'
        ],
        build_by_default : true)
else
    deskcalc_resources_obj = []
endif

deskcalc_app = executable(
    'DeskCalc',
    srcs_deskcalc,
    objects: is_darwin or is_linux ? [deskcalc_resources_obj] : [],
    dependencies: dep_libbe,
    include_directories: common_inc,
    install: true,
    link_args: is_windows ? ['-Wl,--disable-stdcall-fixup', '-Wl,--enable-auto-import', '-Wl,--no-insert-timestamp', '-static-libgcc', '-static-libstdc++', '-lmsvcrt'] : [],
    win_subsystem: is_windows ? 'windows' : 'console',
)

# macOS: Create .icns file from VICN resource and bundle the app
if is_darwin
    deskcalc_icns = custom_target('deskcalc icns',
        input : deskcalc_resources,
        output : 'DeskCalc.icns',
        depends : [vicn2icns_prog],
        command : [vicn2icns_prog, '@INPUT@', '@OUTPUT@'],
        build_by_default : true)
    
    meson.add_install_script('sh', '-c', '''
        APP_NAME="DeskCalc"
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