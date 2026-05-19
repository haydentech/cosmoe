app_name = 'Icon-O-Matic'
linux_desktop_id = 'x-vnd.haiku-icon_o_matic'
linux_desktop_categories = 'Graphics;'
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

iconomatic_includes = [
    'document',
    'document/savers',
    'generic',
    'generic/listener',
    'generic/support',
    'generic/command',
    'generic/gui',
    'generic/gui/panel',
    'generic/gui/panel/color_picker',
    'generic/gui/scrollview',
    'generic/gui/stateview',
    'generic/property',
    'generic/property/specific_properties/',
    'generic/property/view',
    'generic/property/view/specific_properties',
    'generic/selection',
    'generic/property/view/specific_properties',
    'transformable',
    'gui',
    'import_export',
    'import_export/bitmap',
    'import_export/flat_icon',
    'import_export/message',
    'import_export/styled_text',
    'import_export/svg',
    'shape',
    'shape/commands',
    'style',
]

srcs_iconomatic = [
    'main.cpp',
    'MainWindow.cpp',
    'Util.cpp',
    'IconEditorApp.cpp',
    'CanvasView.cpp',
    'document/Document.cpp',
    'document/IconObject.cpp',
    'document/SetPropertiesCommand.cpp',
    'document/savers/AttributeSaver.cpp',
    'document/savers/BitmapSetSaver.cpp',
    'document/savers/DocumentSaver.cpp',
    'document/savers/MessengerSaver.cpp',
    'document/savers/FileSaver.cpp',
    'document/savers/NativeSaver.cpp',
    'document/savers/SimpleFileSaver.cpp',
    'MainWindow.cpp',
    'style/AssignStyleCommand.cpp',
    'style/CurrentColor.cpp',
    'style/MoveStylesCommand.cpp',
    'style/SetColorCommand.cpp',
    'style/AddStylesCommand.cpp',
    'style/RemoveStylesCommand.cpp',
    'style/SetGradientCommand.cpp',
    'generic/command/Command.cpp',
    'generic/command/CommandStack.cpp',
    'generic/command/CompoundCommand.cpp',
    'generic/support/support_settings.cpp',
    'generic/support/Debug.cpp',
    'generic/support/support_ui.cpp',
    'generic/support/support.cpp',
    'generic/listener/Observer.cpp',
    'generic/listener/Observable.cpp',
    'generic/listener/AbstractLOAdapter.cpp',
    'generic/gui/stateview/Manipulator.cpp',
    'generic/gui/stateview/ViewState.cpp',
    'generic/gui/stateview/MultipleManipulatorState.cpp',
    'generic/gui/stateview/StateView.cpp',
    'generic/gui/Group.cpp',
    'generic/gui/NummericalTextView.cpp',
    'generic/gui/InputTextView.cpp',
    'generic/gui/ListViews.cpp',
    #'generic/gui/IconOptionsControl.cpp',
    'generic/gui/SwatchView.cpp',
    'generic/gui/StringTextView.cpp',
    'generic/gui/panel/Panel.cpp',
    'generic/gui/panel/color_picker/ColorPickerPanel.cpp',
    'generic/gui/panel/color_picker/AlphaSlider.cpp',
    'generic/gui/panel/color_picker/ColorSlider.cpp',
    'generic/gui/panel/color_picker/ColorPickerView.cpp',
    'generic/gui/panel/color_picker/ColorField.cpp',
    'generic/gui/panel/color_picker/ColorPreview.cpp',
    # 'generic/gui/popup_control/InputSlider.cpp',
    # 'generic/gui/popup_control/PopupSlider.cpp',
    # 'generic/gui/popup_control/PopupWindow.cpp',
    # 'generic/gui/popup_control/PopupControl.cpp',
    # 'generic/gui/popup_control/PopupView.cpp',
    # 'generic/gui/popup_control/SliderView.cpp',
    'generic/gui/scrollview/Scroller.cpp',
    'generic/gui/scrollview/ScrollableView.cpp',
    'generic/gui/scrollview/ScrollView.cpp',
    'generic/gui/scrollview/Scrollable.cpp',
    'generic/selection/Selectable.cpp',
    'generic/selection/Selection.cpp',
    'generic/property/CommonPropertyIDs.cpp',
    'generic/property/Property.cpp',
    'generic/property/view/PropertyItemView.cpp',
    'generic/property/view/PropertyListView.cpp',
    'generic/property/view/PropertyEditorFactory.cpp',
    'generic/property/view/PropertyEditorView.cpp',
    'generic/property/view/specific_properties/StringValueView.cpp',
    'generic/property/view/specific_properties/ColorValueView.cpp',
    'generic/property/view/specific_properties/BoolValueView.cpp',
    'generic/property/view/specific_properties/TextInputValueView.cpp',
    'generic/property/view/specific_properties/FloatValueView.cpp',
    'generic/property/view/specific_properties/OptionValueView.cpp',
    'generic/property/view/specific_properties/IconValueView.cpp',
    'generic/property/view/specific_properties/IntValueView.cpp',
    'generic/property/view/specific_properties/Int64ValueView.cpp',
    'generic/property/view/specific_properties/SwatchValueView.cpp',
    'generic/property/PropertyObject.cpp',
    'generic/property/specific_properties/Int64Property.cpp',
    'generic/property/specific_properties/IconProperty.cpp',
    'generic/property/specific_properties/OptionProperty.cpp',
    'generic/property/specific_properties/ColorProperty.cpp',
    'shape/PathManipulator.cpp',
    'shape/commands/MoveShapesCommand.cpp',
    'shape/commands/ChangePointCommand.cpp',
    'shape/commands/ReversePathCommand.cpp',
    'shape/commands/RotatePathIndicesCommand.cpp',
    'shape/commands/SplitPointsCommand.cpp',
    'shape/commands/RemovePointsCommand.cpp',
    'shape/commands/InsertPointCommand.cpp',
    'shape/commands/AddPointCommand.cpp',
    'shape/commands/AddTransformersCommand.cpp',
    'shape/commands/MovePathsCommand.cpp',
    'shape/commands/CleanUpPathCommand.cpp',
    'shape/commands/PathCommand.cpp',
    'shape/commands/RemovePathsCommand.cpp',
    'shape/commands/TransformPointsCommand.cpp',
    'shape/commands/RemoveTransformersCommand.cpp',
    'shape/commands/AddShapesCommand.cpp',
    'shape/commands/UnassignPathCommand.cpp',
    'shape/commands/AddPathsCommand.cpp',
    'shape/commands/NudgePointsCommand.cpp',
    'shape/commands/FreezeTransformationCommand.cpp',
    'shape/commands/RemoveShapesCommand.cpp',
    'shape/commands/MoveTransformersCommand.cpp',
    'shape/commands/FlipPointsCommand.cpp',
    'import_export/svg/DocumentBuilder.cpp',
    'import_export/svg/SVGExporter.cpp',
    'import_export/svg/SVGImporter.cpp',
    'import_export/bitmap/BitmapExporter.cpp',
    'import_export/Exporter.cpp',
    'import_export/Importer.cpp',
    'import_export/flat_icon/SourceExporter.cpp',
    'import_export/flat_icon/FlatIconExporter.cpp',
    'import_export/flat_icon/RDefExporter.cpp',
    'import_export/message/MessageExporter.cpp',
    'import_export/styled_text/StyledTextImporter.cpp',
    'gui/IconView.cpp',
    'gui/PathListView.cpp',
    'gui/GradientControl.cpp',
    'gui/StyleView.cpp',
    'gui/IconObjectListView.cpp',
    'gui/StyleListView.cpp',
    'gui/ShapeListView.cpp',
    'gui/SwatchGroup.cpp',
    'gui/SavePanel.cpp',
    'gui/TransformerListView.cpp',
    'transformable/TransformObjectsCommand.cpp',
    'transformable/PerspectiveBoxStates.cpp',
    'transformable/ResetTransformationCommand.cpp',
    'transformable/TransformGradientCommand.cpp',
    'transformable/TransformShapesBox.cpp',
    'transformable/PerspectiveBox.cpp',
    'transformable/TransformCommand.cpp',
    'transformable/CanvasTransformBox.cpp',
    'transformable/TransformPointsBox.cpp',
    'transformable/PerspectiveCommand.cpp',
    'transformable/TransformBox.cpp',
    'transformable/TransformGradientBox.cpp',
    'transformable/ChannelTransform.cpp',
    'transformable/TransformBoxStates.cpp',
]

app_resource_defs = [
    'Icon-O-Matic.rdef'
]

# Compile .rdef to .rsrc
app_resources = custom_target('iconomatic resources',
    depends : rc_app_dep,
    input : app_resource_defs,
    output : 'Icon-O-Matic.rsrc',
    command : [rc_app, '@INPUT@', '-o', '@OUTPUT@', '-I', '.'],
    build_by_default : true)

# Create a .o file with the resources embedded as a section
if is_darwin
    # Step 1: Create empty C file and compile it
    app_resources_stub = custom_target('app resources stub',
        output : app_name + '_stub.o',
        command : [
            'sh', '-c',
            'echo "void _cosmoe_rsrc_stub_@0@(void) {}" | '.format(app_name.replace('-', '_')) +
            cc.cmd_array()[0] + ' -x c - -c -o @OUTPUT@'
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
    # Linux: Use ld -r -b binary to create a .o file, rename section to .cosmoe_rsrc, and add GNU-stack note
    app_resources_obj = custom_target(app_name + ' resources object',
        input : app_resources,
        output : app_name + '_rsrc.o',
        command : [
            'sh', '-c',
            # Create object with embedded binary (use basename so symbols don't include path)
            'INPUT_DIR=$(dirname "$1") && INPUT_FILE=$(basename "$1") && OUTPUT_ABS=$(cd $(dirname "$2") && pwd)/$(basename "$2") && ' +
            'cd "$INPUT_DIR" && ld -r -b binary "$INPUT_FILE" -o "$OUTPUT_ABS" && ' +
            # Rename .data section so it does not collide with executable .data
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

iconomatic_app = executable(
    'Icon-O-Matic',
    srcs_iconomatic,
    objects: is_windows ? [app_resources_obj, app_icon_obj] : [app_resources_obj],
    cpp_args: [
        '-DICON_O_MATIC=1',
        '-D__HAIKU__'
    ],
    link_with: [
        lib_libicon_iconomatic,
        lib_libbe,
        lib_libtracker,
    ],
    dependencies: [
        dep_libbe,
        dep_libtracker,
        dep_libicon_iconomatic
    ],
    include_directories: [
        icon_inc,
        common_inc,
        iconomatic_includes,
    ],
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
    iconomatic_icns = custom_target(app_name + ' icns',
        input : app_resources,
        output : app_name + '.icns',
        depends : [vicn2icns_prog],
        command : [vicn2icns_prog, '@INPUT@', '@OUTPUT@'],
        build_by_default : true)

    meson.add_install_script('sh', '-c', '''
        APP_NAME="''' + app_name + '''"
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
        APP_NAME_LOWER="icon-o-matic"
        
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