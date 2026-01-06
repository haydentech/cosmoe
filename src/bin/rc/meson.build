

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

# rc is only built for native (non-cross) builds because:
# 1. It needs to run during the build to process .rdef files
# When cross-compiling, resource compilation is skipped
if not meson.is_cross_build()
    rc_app = executable(
        'rc',
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
        install: true,
    )
else
    # Create a dummy target for cross-builds so other build files can reference it
    rc_app = disabler()
endif
