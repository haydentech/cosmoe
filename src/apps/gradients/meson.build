srcs_gradients = [
    'GradientsApp.cpp',
    'GradientsView.cpp',
    'GradientsWindow.cpp',
]


gradients_app = executable(
    'Gradients',
    srcs_gradients,
    dependencies: dep_libbe,
    include_directories: common_inc,
    install: true,
)

srcs_gradients_resources = [
    'Gradients.rdef'
]

gradients_resources = custom_target('gradients resources',
    depends : [gradients_app, rc_app],
    input : srcs_gradients_resources,
    output : 'Gradients.rsrc',
    command : [rc_app, '@INPUT@', '-o', '@OUTPUT@', '-I', '.'],
    build_by_default : true)

custom_target('gradients plus resources',
    depends : gradients_resources,
    input : gradients_app,
    output : 'fake',
    command : [xres_app, '-o', '@INPUT@', gradients_resources],
    build_by_default : true)