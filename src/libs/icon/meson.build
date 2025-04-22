
srcs_libicon = [
    'Icon.cpp',
    'IconRenderer.cpp',
    'IconUtils.cpp',

	'flat_icon/FlatIconFormat.cpp',
    'flat_icon/FlatIconImporter.cpp',
	'flat_icon/LittleEndianBuffer.cpp',
    'flat_icon/PathCommandQueue.cpp',

    'transformer/AffineTransformer.cpp',
	'transformer/CompoundStyleTransformer.cpp',
    'transformer/ContourTransformer.cpp',
    'transformer/PerspectiveTransformer.cpp',
    'transformer/StrokeTransformer.cpp',
    'transformer/StyleTransformer.cpp',
    'transformer/PathSource.cpp',
    'transformer/TransformerFactory.cpp',

    'message/Defines.cpp',
	'message/MessageImporter.cpp',

	'shape/Shape.cpp',

    'style/Style.cpp',
	'style/GradientTransformable.cpp',

    'shape/PathSourceShape.cpp',
	'shape/VectorPath.cpp',

	'transformable/Transformable.cpp',
]

lib_libicon = static_library(
    'icon',
    srcs_libicon,
    include_directories: ['generic', 'transformable', 'flat_icon', 'message', 'shape', 'style', 'transformer', common_inc],
    c_args: ['-O2'],
    install: false,
)

dep_libicon = declare_dependency(
    link_with: lib_libicon,
    include_directories: common_inc,
)