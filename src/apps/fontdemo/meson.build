srcs_fontdemo = [
    'ControlView.cpp',
    'FontDemo.cpp',
    'FontDemoView.cpp',
]

fontdemo_app = executable(
    'FontDemo',
    srcs_fontdemo,
    objects: is_darwin or is_linux ? [app_resources_obj] : [],
    link_with: [
        lib_libbe,
    ],
    dependencies: [
        dep_libbe
    ],
    include_directories: common_inc,
    install: true,
    link_args: is_windows ? ['-Wl,--disable-stdcall-fixup', '-Wl,--enable-auto-import', '-Wl,--no-insert-timestamp', '-static-libgcc', '-static-libstdc++', '-lmsvcrt'] : [],
    win_subsystem: is_windows ? 'windows' : 'console',
)

app_resource_defs = [
    'FontDemo.rdef'
]

app_resources = custom_target('fontdemo resources',
    depends : [rc_app],
    input : app_resource_defs,
    output : 'FontDemo.rsrc',
    command : [rc_app, '@INPUT@', '-o', '@OUTPUT@', '-I', '.'],
    build_by_default : true)

# Create a .o file with the resources embedded as a section
if is_darwin
    # macOS: Strip the first 4 bytes from .rsrc file
    app_resources_stripped = custom_target('fontdemo resources stripped',
        input : app_resources,
        output : 'FontDemo_rsrc_data',
        command : ['dd', 'if=@INPUT@', 'of=@OUTPUT@', 'bs=4', 'skip=1', 'status=none'],
        build_by_default : true)
    
    app_resources_stub = custom_target('fontdemo resources stub',
        output : 'FontDemo_stub.o',
        command : [
            'sh', '-c',
            'echo "void _cosmoe_rsrc_stub_FontDemo(void) {}" | ' +
            cc.cmd_array()[0] + ' -x c - -c -o @OUTPUT@'
        ],
        build_by_default : true)
    
    app_resources_obj = custom_target('fontdemo resources object',
        input : app_resources_stripped,
        output : 'FontDemo_rsrc.o',
        depends : app_resources_stub,
        command : [
            'ld', '-r',
            app_resources_stub,
            '-sectcreate', '__TEXT', '__rsrc', '@INPUT@',
            '-o', '@OUTPUT@'
        ],
        build_by_default : true)
elif is_linux
    app_resources_obj = custom_target('fontdemo resources object',
        input : app_resources,
        output : 'FontDemo_rsrc.o',
        command : [
            'ld', '-r', '-b', 'binary',
            '@INPUT@',
            '-o', '@OUTPUT@'
        ],
        build_by_default : true)
else
    app_resources_obj = []
endif

# macOS app bundle support
if is_darwin
    fontdemo_icns = custom_target('fontdemo icns',
        input : app_resources,
        output : 'FontDemo.icns',
        depends : [vicn2icns_prog],
        command : [vicn2icns_prog, '@INPUT@', '@OUTPUT@'],
        build_by_default : true)
    
    meson.add_install_script('sh', '-c', '''
        APP_NAME="FontDemo"
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