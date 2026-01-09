

flex = find_program('flex', required: true)
bison = find_program('bison', required: true)

lgen = generator(flex,
    output : '@PLAINNAME@.yy.cpp',
    arguments : ['-o', '@OUTPUT@', '@INPUT@'])

lfiles = lgen.process('lexer.l')

bgen = generator(bison,
    output : ['@BASENAME@.tab.cpp', '@BASENAME@.hpp'],
    arguments : ['@INPUT@', '--defines=@OUTPUT1@', '--output=@OUTPUT0@'])

bfiles = bgen.process('parser.y')

srcs_rc = [
    'compile.cpp',
    'decompile.cpp',
    'rc.cpp',
    'rdef.cpp',
]

# When cross-compiling, use the already-installed native RC tool
# When building natively, build RC from source
if meson.is_cross_build()
    # Use the installed native rc tool (e.g., /usr/local/bin/rc)
    rc_app = find_program('rc', native: true, required: true)
    # For cross-builds, rc_app is an ExternalProgram and can't be in depends
    rc_app_dep = []
    message('Cross-compiling: Using native rc tool: ' + rc_app.full_path())
else
    # Build rc-bootstrap from source for native builds
    # This version links to libbe-bootstrap and is used ONLY during build
    # It no longer depends on libbe - uses standalone resource classes
    # Don't include common_inc to avoid conflicts with system headers
    rc_app = executable(
        'rc-bootstrap',
        srcs_rc,
        lfiles,
        bfiles,
        link_with: [
            lib_libroot,
            lib_libbe_bootstrap,
        ],
        dependencies: [
            dep_libbe_bootstrap,
        ],
        include_directories: common_inc,
        install: false,  # Don't install bootstrap version
    )
    # For native builds, rc_app is a BuildTarget and CAN be in depends
    rc_app_dep = [rc_app]
    message('Native build: Building rc-bootstrap from source (for build-time use)')
endif

