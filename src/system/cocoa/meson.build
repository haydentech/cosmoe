# Cocoa backend plugin for Cosmoe on macOS

cocoa_sources = files([
    'window.m',
])

cocoa_backend_sources = files([
    'CocoaBackend.mm',
])

# macOS frameworks required for Cocoa backend
cocoa_frameworks = [
    '-framework', 'Cocoa',
    '-framework', 'QuartzCore',
    '-framework', 'CoreGraphics',
]

# Cairo with Quartz support is needed
dep_cairo_quartz = dependency('cairo-quartz', required: false)
if not dep_cairo_quartz.found()
    # Fall back to regular cairo
    dep_cairo_quartz = dependency('cairo')
endif

cocoa_backend = shared_module(
    'cosmoe-cocoa',
    [cocoa_backend_sources, cocoa_sources],
    include_directories: [include_directories('.'), common_inc],
    dependencies: [
        dep_cairo_quartz,
        dependency('cairo'),
    ],
    link_args: cocoa_frameworks,
    install: true,
    install_dir: '/usr/local/lib',
    name_prefix: 'lib',
)
