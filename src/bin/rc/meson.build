

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

rc_app = executable(
    'rc',
    srcs_rc,
    lfiles,
    bfiles,
    link_with: [
        lib_libbe,
    ],
    dependencies: [
        dep_libbe,
    ],
    include_directories: common_inc,
    install: true,
)
