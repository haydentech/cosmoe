srcs_deskcalc = [
    'CalcApplication.cpp',
    'CalcOptions.cpp',
    'CalcView.cpp',
    'CalcWindow.cpp',
    'DeskCalc.cpp',
    'ExpressionTextView.cpp',
    'InputTextView.cpp',
]


deskcalc_app = executable(
    'DeskCalc',
    srcs_deskcalc,
    dependencies: dep_libbe,
    include_directories: common_inc,
    install: true,
)

srcs_deskcalc_resources = [
    'DeskCalc.rdef'
]

deskcalc_resources = custom_target('deskcalc resources',
    depends : deskcalc_app,
    input : srcs_deskcalc_resources,
    output : 'DeskCalc.rsrc',
    command : ['rc', '@INPUT@', '-o', '@OUTPUT@', '-I', '.'],
    build_by_default : true)

custom_target('deskcalc plus resources',
    depends : deskcalc_resources,
    input : deskcalc_app,
    output : 'fake',
    command : ['xres', '-o', '@INPUT@', deskcalc_resources],
    build_by_default : true)