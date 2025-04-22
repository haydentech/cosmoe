
srcs_linprog = [
    'ActiveSetSolver.cpp',
    'Constraint.cpp',
    'LayoutOptimizer.cpp',
    'LinearSpec.cpp',
    'Summand.cpp',
    'Variable.cpp',
]

lib_linprog = static_library(
    'linprog',
    srcs_linprog,
    include_directories: common_inc,
    c_args: ['-O2'],
    install: false,
)

dep_linprog = declare_dependency(
    link_with: lib_linprog,
    include_directories: common_inc,
)