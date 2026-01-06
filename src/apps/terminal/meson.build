# Terminal app only makes sense on Unix-like systems with PTY support
if not is_windows

srcs_terminal = [
    'ActiveProcessInfo.cpp',
    'AppearPrefView.cpp',
    'Arguments.cpp',
    'BasicTerminalBuffer.cpp',
    'Colors.cpp',
    'FindWindow.cpp',
    'Globals.cpp',
    'HistoryBuffer.cpp',
    'HyperLink.cpp',
    'InlineInput.cpp',
    'PatternEvaluator.cpp',
    'PrefHandler.cpp',
    'PrefWindow.cpp',
    'SetTitleDialog.cpp',
    'Shell.cpp',
    'ShellInfo.cpp',
    'ShellParameters.cpp',
    'SmartTabView.cpp',
    'TermApp.cpp',
    'TermConst.cpp',
    'TerminalBuffer.cpp',
    'TerminalCharClassifier.cpp',
    'TerminalRoster.cpp',
    'TermParse.cpp',
    'TermScrollView.cpp',
    'TermView.cpp',
    'TermViewHighlight.cpp',
    'TermViewStates.cpp',
    'TermWindow.cpp',
    'ThemeView.cpp',
    'ThemeWindow.cpp',
    'TitlePlaceholderMapper.cpp',
    'VTKeyTbl.c',
    'VTPrsTbl.c',
]

terminal_app = executable(
    'Terminal',
    srcs_terminal,
    objects: is_darwin or is_linux ? [app_resources_obj] : [],
    dependencies: [dep_libbe, dep_libtracker],
    include_directories: common_inc,
    install: true,
)

srcs_terminal_resources = [
    'Terminal.rdef',
    'XColors.rdef'
]

terminal_resources = custom_target('terminal resources',
    depends : [rc_app],
    input : srcs_terminal_resources,
    output : 'Terminal.rsrc',
    command : [rc_app, '@INPUT@', '-o', '@OUTPUT@', '-I', '.'],
    build_by_default : true)

# Create a .o file with the resources embedded as a section
if is_darwin
    # macOS: Strip the first 4 bytes from .rsrc file
    app_resources_stripped = custom_target('terminal resources stripped',
        input : terminal_resources,
        output : 'Terminal_rsrc_data',
        command : ['dd', 'if=@INPUT@', 'of=@OUTPUT@', 'bs=4', 'skip=1', 'status=none'],
        build_by_default : true)
    
    app_resources_stub = custom_target('terminal resources stub',
        output : 'Terminal_stub.o',
        command : [
            'sh', '-c',
            'echo "void _cosmoe_rsrc_stub_Terminal(void) {}" | ' +
            cc.cmd_array()[0] + ' -x c - -c -o @OUTPUT@'
        ],
        build_by_default : true)
    
    app_resources_obj = custom_target('terminal resources object',
        input : app_resources_stripped,
        output : 'Terminal_rsrc.o',
        depends : app_resources_stub,
        command : [
            'ld', '-r',
            app_resources_stub,
            '-sectcreate', '__TEXT', '__rsrc', '@INPUT@',
            '-o', '@OUTPUT@'
        ],
        build_by_default : true)
elif is_linux
    app_resources_obj = custom_target('terminal resources object',
        input : terminal_resources,
        output : 'Terminal_rsrc.o',
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
    terminal_icns = custom_target('terminal icns',
        input : terminal_resources,
        output : 'Terminal.icns',
        depends : [vicn2icns_prog],
        command : [vicn2icns_prog, '@INPUT@', '@OUTPUT@'],
        build_by_default : true)
    
    meson.add_install_script('sh', '-c', '''
        APP_NAME="Terminal"
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

endif  # not is_windows