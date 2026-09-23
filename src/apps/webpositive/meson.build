app_name = 'WebPositive'
linux_desktop_id = 'x-vnd.Haiku-WebPositive'
linux_desktop_categories = 'Network;WebBrowser;'
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

webkit_headers = [
    'WebView.h',
    'WebWindow.h',
    'WebPage.h',
    'WebSettings.h',
    'WebKitInfo.h',
    'WebDownload.h',
    'WebViewConstants.h',
]

webkit_headers_found = true
foreach header : webkit_headers
    if not cpp.has_header(header)
        webkit_headers_found = false
    endif
endforeach

webkit_dep = disabler()
webkit_dep_found = false
foreach candidate : ['WebKitLegacy']
    candidate_dep = cpp.find_library(candidate, required : false)
    if candidate_dep.found()
        webkit_dep = candidate_dep
        webkit_dep_found = true
        break
    endif
endforeach

webpositive_build_rpath = ''
webpositive_install_rpath = ''
if is_linux
    prefix = get_option('prefix')
    libdir = get_option('libdir')
    if libdir.startswith('/')
        webpositive_build_rpath = meson.project_build_root() + ':' + libdir
        webpositive_install_rpath = libdir
    else
        webpositive_build_rpath = meson.project_build_root() + ':' + prefix / libdir
        webpositive_install_rpath = prefix / libdir
    endif
endif

# webpositive_option = get_option('enable_webpositive')
# webpositive_available = webkit_headers_found and webkit_dep_found

# if webpositive_option.disabled()
#     message('WebPositive disabled by option')
# elif not webpositive_available
#     if webpositive_option.enabled()
#         error('WebPositive requested, but HaikuWebKit headers or library were not found')
#     endif
#     message('Skipping WebPositive: HaikuWebKit headers or library not found')
# else
    app_srcs = files(
        'AuthenticationPanel.cpp',
        'BookmarkBar.cpp',
        'BrowserApp.cpp',
        'BrowserWindow.cpp',
        'BrowsingHistory.cpp',
        'ConsoleWindow.cpp',
        'CookieWindow.cpp',
        'CredentialsStorage.cpp',
        'DownloadProgressView.cpp',
        'DownloadWindow.cpp',
        'SettingsKeys.cpp',
        'SettingsWindow.cpp',
        'URLInputGroup.cpp',
        'autocompletion/AutoCompleter.cpp',
        'autocompletion/AutoCompleterDefaultImpl.cpp',
        'autocompletion/TextViewCompleter.cpp',
        'support/BaseURL.cpp',
        'support/FontSelectionView.cpp',
        'tabview/TabContainerView.cpp',
        'tabview/TabManager.cpp',
        'tabview/TabView.cpp',
    )

    app_resource_defs = files(
        app_name + '.rdef'
    )

    app_resources = custom_target('webpositive resources',
        depends : rc_app_dep,
        input : app_resource_defs,
        output : app_name + '.rsrc',
        command : [rc_app, '@INPUT@', '-o', '@OUTPUT@', '-I', '.', '-I', meson.current_source_dir()],
        build_by_default : true)

    if is_darwin
        app_resources_stub = custom_target('webpositive resources stub',
            output : app_name + '_stub.o',
            command : [
                'sh', '-c',
                'echo "void _cosmoe_rsrc_stub_@0@(void) {}" | '.format(app_name) +
                cc.cmd_array()[0] + ' -x c - -c -o @OUTPUT@'
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

        app_ico = custom_target('webpositive ico',
            input : app_resources,
            output : app_name + '.ico',
            depends : [vicn2ico_prog],
            command : [vicn2ico_prog, '@INPUT@', '@OUTPUT@'],
            build_by_default : true)

        app_rc = custom_target('webpositive rc',
            output : app_name + '.rc',
            input : app_ico,
            capture : true,
            command : ['echo', 'IDI_ICON1 ICON "' + app_name + '.ico"'],
            build_by_default : true)

        app_icon_obj = custom_target('webpositive icon object',
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

    webpositive_inc = include_directories([
        '.',
        'autocompletion',
        'support',
        'tabview',
    ])

    app_include_directories += [common_inc, tracker_inc, webpositive_inc]

    app_deps = [
        dep_libbe,
        dep_libtracker,
        dep_libnetapi,
        dep_libnetservices,
        webkit_dep,
    ]

    app_dependencies = app_deps

    webpositive_app = executable(
        app_name,
        app_srcs,
        objects : is_windows ? [app_resources_obj, app_icon_obj] : [app_resources_obj],
        dependencies : app_deps,
        include_directories : [common_inc, tracker_inc, webpositive_inc],
        cpp_args : [
            '-Wno-error=sequence-point',
            '-Wno-error=format-truncation',
        ],
        install : true,
        build_rpath : webpositive_build_rpath,
        install_rpath : webpositive_install_rpath,
        link_args : is_windows ? ['-Wl,--disable-stdcall-fixup', '-Wl,--enable-auto-import', '-Wl,--no-insert-timestamp', '-static-libgcc', '-static-libstdc++', '-lmsvcrt'] : [],
        win_subsystem : is_windows ? 'windows' : 'console',
    )

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
        webpositive_icns = custom_target('webpositive icns',
            input : app_resources,
            output : app_name + '.icns',
            depends : [vicn2icns_prog],
            command : [vicn2icns_prog, '@INPUT@', '@OUTPUT@'],
            build_by_default : true)

        meson.add_install_script('sh', '-c', '''
            APP_NAME="'''+ app_name + '''"
            BUNDLE_DIR="$DESTDIR/usr/local/Applications/${APP_NAME}.app"
            mkdir -p "${BUNDLE_DIR}/Contents/MacOS"
            mkdir -p "${BUNDLE_DIR}/Contents/Resources"
            mv "$DESTDIR/usr/local/bin/${APP_NAME}" "${BUNDLE_DIR}/Contents/MacOS/"
            cp "''' + meson.current_build_dir() + '''/${APP_NAME}.icns" "${BUNDLE_DIR}/Contents/Resources/"
            cat > "${BUNDLE_DIR}/Contents/Info.plist" << PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD plist 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
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

    if is_linux
        meson.add_install_script('sh', '-c', '''
            RSRC_FILE="''' + meson.current_build_dir() + '/' + app_name + '''.rsrc"
            ICON_DIR="$DESTDIR/usr/local/share/icons"
            APP_NAME_LOWER="webpositive"

            "''' + meson.global_build_root() + '''/vicn2png" \
                "${RSRC_FILE}" \
                "${ICON_DIR}" \
                "${APP_NAME_LOWER}"

            if command -v gtk-update-icon-cache >/dev/null 2>&1; then
                gtk-update-icon-cache -f -t "${ICON_DIR}/hicolor" 2>/dev/null || true
            fi
        ''')
    endif
# endif

    app_enabled = false