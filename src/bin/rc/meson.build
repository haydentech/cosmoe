

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

# files() so the same list can be reused by the installed rc, which is linked
# from the top-level meson.build.
srcs_rc = files(
    'compile.cpp',
    'decompile.cpp',
    'rc.cpp',
    'rdef.cpp',
)

# rc runs while building: it compiles the .rdef resources that libbe itself
# embeds, and those of every app.  That makes it a build-machine tool in every
# configuration - including a cross build, where the target libbe cannot exist
# until rc has run.  It links the build-machine libbe (libbe_tool), so nothing
# has to be built, installed or passed in beforehand.
rc_app = executable(
    'rc-bootstrap',
    srcs_rc,
    lfiles,
    bfiles,
    dependencies: [dep_libbe_tool],
    include_directories: common_inc,
    # A native build has a single machine, so there is nothing to distinguish
    native: meson.is_cross_build(),
    install: false,  # The installed rc is built with the real libbe below
)
rc_app_dep = [rc_app]
message('Building build-machine rc for resource compilation')

