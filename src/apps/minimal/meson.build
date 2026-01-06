app_name = 'minimal'

srcs_minimal = [
    'app.cpp',
    'disapp.cpp',
    'diswindow.cpp',
    'disview.cpp'
]

minimal_app = executable(
    app_name,
    srcs_minimal,
    objects: is_darwin or is_linux ? [app_resources_obj] : [],
    link_with: [ lib_libbe ],
    dependencies: [ dep_libbe ],
    link_args: is_windows ? ['-Wl,--enable-runtime-pseudo-reloc', '-Wl,--enable-auto-import'] : [],
    include_directories: common_inc,
    win_subsystem: is_windows ? 'windows' : 'console',
    install: true,
)

app_resource_defs = [
    app_name + '.rdef'
]

app_resources = custom_target('minimal resources',
    depends : [rc_app],
    input : app_resource_defs,
    output : app_name + '.rsrc',
    command : [rc_app, '@INPUT@', '-o', '@OUTPUT@', '-I', '.'],
    build_by_default : true)

# Create a .o file with the resources embedded as a section
if is_darwin
    # macOS: Strip the first 4 bytes from .rsrc file
    app_resources_stripped = custom_target('minimal resources stripped',
        input : app_resources,
        output : app_name + '_rsrc_data',
        command : ['dd', 'if=@INPUT@', 'of=@OUTPUT@', 'bs=4', 'skip=1', 'status=none'],
        build_by_default : true)
    
    app_resources_stub = custom_target('minimal resources stub',
        output : app_name + '_stub.o',
        command : [
            'sh', '-c',
            'echo "void _cosmoe_rsrc_stub_@0@(void) {}" | '.format(app_name) +
            cc.cmd_array()[0] + ' -x c - -c -o @OUTPUT@'
        ],
        build_by_default : true)
    
    app_resources_obj = custom_target('minimal resources object',
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
    app_resources_obj = custom_target('minimal resources object',
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

# macOS app bundle support
if is_darwin
    minimal_icns = custom_target('minimal icns',
        input : app_resources,
        output : 'minimal.icns',
        depends : [vicn2icns_prog],
        command : [vicn2icns_prog, '@INPUT@', '@OUTPUT@'],
        build_by_default : true)
    
    meson.add_install_script('sh', '-c', '''
        APP_NAME="minimal"
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
