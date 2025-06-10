
srcs_libicon = [
	'flat_icon/FlatIconFormat.cpp',
    'flat_icon/FlatIconImporter.cpp',
	'flat_icon/LittleEndianBuffer.cpp',
    'flat_icon/PathCommandQueue.cpp',

    'message/Defines.cpp',
	'message/MessageImporter.cpp',

    'shape/PathSourceShape.cpp',
    'shape/ReferenceImage.cpp',
	'shape/Shape.cpp',
	'shape/VectorPath.cpp',

	'style/GradientTransformable.cpp',
    'style/Style.cpp',

	'transformable/Transformable.cpp',

    'transformer/AffineTransformer.cpp',
	'transformer/CompoundStyleTransformer.cpp',
    'transformer/ContourTransformer.cpp',
    'transformer/PathSource.cpp',
    'transformer/PerspectiveTransformer.cpp',
    'transformer/StrokeTransformer.cpp',
    'transformer/StyleTransformer.cpp',
    'transformer/TransformerFactory.cpp',

    'Icon.cpp',
    'IconRenderer.cpp',
]

libicon_inc = [
    'generic',
    'transformable',
    'flat_icon',
    'message',
    'shape',
    'style',
    'transformer',
]

lib_libicon = static_library(
    'icon',
    [srcs_libicon, 'IconUtils.cpp'],
    include_directories: [libicon_inc, common_inc],
    cpp_args: ['-O2',
        '-D__HAIKU__',
    ],
    install: false,
)


dep_libicon = declare_dependency(
    link_with: lib_libicon,
    include_directories: common_inc,
)

# Now do the whole thing again, minus IconUtils.cpp and with a custom define, for the Icon-O-Matic version

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


iconomatic_inc_relocated = []

foreach inc : iconomatic_includes
  iconomatic_inc_relocated += '../../apps/icon-o-matic/' + inc
endforeach

lib_libicon_iconomatic = static_library(
    'icon_iconomatic',
    srcs_libicon,
    include_directories: [libicon_inc, common_inc, iconomatic_inc_relocated],
    cpp_args: [
        '-O2',
        '-DICON_O_MATIC',
        '-D__HAIKU__',
    ],
    install: false,
)

dep_libicon_iconomatic = declare_dependency(
    link_with: lib_libicon_iconomatic,
    include_directories: common_inc,
)

