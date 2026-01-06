srcs_gradients = [
    'GradientsApp.cpp',
    'GradientsView.cpp',
    'GradientsWindow.cpp',
]


gradients_app = executable(
    'Gradients',
    srcs_gradients,
    objects: is_darwin or is_linux ? [app_resources_obj] : [],
    dependencies: dep_libbe,
    install: true,
    link_args: is_windows ? ['-Wl,--disable-stdcall-fixup', '-Wl,--enable-auto-import', '-Wl,--no-insert-timestamp', '-static-libgcc', '-static-libstdc++', '-lmsvcrt'] : [],
    win_subsystem: is_windows ? 'windows' : 'console',
)

srcs_gradients_resources = [
    'Gradients.rdef'
]

gradients_resources = custom_target('gradients resources',
    depends : [gradients_app, rc_app],
    input : srcs_gradients_resources,
    output : 'Gradients.rsrc',
    command : [rc_app, '@INPUT@', '-o', '@OUTPUT@', '-I', '.'],
    build_by_default : true)
# macOS: Create .icns file from VICN resource and bundle the app
if is_darwin
    gradients_icns = custom_target('gradients icns',
        input : gradients_resources,
        output : 'Gradients.icns',
        depends : [vicn2icns_prog],
        command : [vicn2icns_prog, '@INPUT@', '@OUTPUT@'],
        build_by_default : true)
    
    meson.add_install_script('sh', '-c', '''
        APP_NAME="Gradients"
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