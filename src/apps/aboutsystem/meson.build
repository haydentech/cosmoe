srcs_aboutsystem = [
    'AboutSystem.cpp',
    'HyperTextActions.cpp',
    'HyperTextView.cpp',
    'Utilities.cpp',
]

cosmoe_version = run_command('date', '+%F', check: true).stdout().strip()

srcs_aboutsystem_resources = [
    'AboutSystem.rdef',
]

# Move the artwork to the build directory so the resource compiler can find it
resource_artwork = custom_target('resource_artwork',
    input: [meson.global_source_root() + '/data/artwork/Walter logo.png'],
    output: ['res'],
    command: ['cp', '-r', meson.global_source_root() + '/data/artwork', meson.global_build_root() + '/data/'],
    install: false,
    build_by_default: true
)

# Compile .rdef to .rsrc
aboutsystem_resources = custom_target('aboutsystem resources',
    depends : [rc_app, resource_artwork],
    depend_files : meson.global_source_root() + '/data/artwork/Walter logo.png',
    input : srcs_aboutsystem_resources,
    output : 'AboutSystem.rsrc',
    command : [rc_app, '@INPUT@', '-o', '@OUTPUT@', '-I', '.', '-I', meson.global_build_root() + '/data/artwork'],
    build_by_default : true)

# Create a .o file with the resources embedded as a section
if is_darwin
    # macOS: Strip the first 4 bytes from .rsrc file
    aboutsystem_resources_stripped = custom_target('aboutsystem resources stripped',
        input : aboutsystem_resources,
        output : 'AboutSystem_rsrc_data',
        command : ['dd', 'if=@INPUT@', 'of=@OUTPUT@', 'bs=4', 'skip=1', 'status=none'],
        build_by_default : true)
    
    aboutsystem_resources_stub = custom_target('aboutsystem resources stub',
        output : 'AboutSystem_stub.o',
        command : [
            'sh', '-c',
            'echo "void _cosmoe_rsrc_stub_AboutSystem(void) {}" | ' +
            cc.cmd_array()[0] + ' -x c - -c -o @OUTPUT@'
        ],
        build_by_default : true)
    
    aboutsystem_resources_obj = custom_target('aboutsystem resources object',
        input : aboutsystem_resources_stripped,
        output : 'AboutSystem_rsrc.o',
        depends : aboutsystem_resources_stub,
        command : [
            'ld', '-r',
            aboutsystem_resources_stub,
            '-sectcreate', '__TEXT', '__rsrc', '@INPUT@',
            '-o', '@OUTPUT@'
        ],
        build_by_default : true)
elif is_linux
    aboutsystem_resources_obj = custom_target('aboutsystem resources object',
        input : aboutsystem_resources,
        output : 'AboutSystem_rsrc.o',
        command : [
            'ld', '-r', '-b', 'binary',
            '@INPUT@',
            '-o', '@OUTPUT@'
        ],
        build_by_default : true)
else
    aboutsystem_resources_obj = []
endif

aboutsystem_app = executable(
    'AboutSystem',
    srcs_aboutsystem,
    objects: is_darwin or is_linux ? [aboutsystem_resources_obj] : [],
    cpp_args: ['-DCOSMOE_VERSION=(built ' + cosmoe_version + ')'],
    dependencies: dep_libbe,
    include_directories: common_inc,
    install: true,
    link_args: is_windows ? ['-Wl,--disable-stdcall-fixup', '-Wl,--enable-auto-import', '-Wl,--no-insert-timestamp', '-static-libgcc', '-static-libstdc++', '-lmsvcrt'] : [],
    win_subsystem: is_windows ? 'windows' : 'console',
)
# macOS: Create .icns file from VICN resource and bundle the app
if is_darwin
    aboutsystem_icns = custom_target('aboutsystem icns',
        input : aboutsystem_resources,
        output : 'AboutSystem.icns',
        depends : [vicn2icns_prog],
        command : [vicn2icns_prog, '@INPUT@', '@OUTPUT@'],
        build_by_default : true)
    
    meson.add_install_script('sh', '-c', '''
        APP_NAME="AboutSystem"
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
endif
