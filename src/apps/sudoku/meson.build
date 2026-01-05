srcs_sudoku = [
    'ProgressWindow.cpp',
    'Sudoku.cpp',
    'SudokuField.cpp',
    'SudokuGenerator.cpp',
    'SudokuView.cpp',
    'SudokuWindow.cpp',
    'SudokuSolver.cpp',
]


sudoku_app = executable(
    'Sudoku',
    srcs_sudoku,
    dependencies: [dep_libbe, dep_libtracker],
    include_directories: [common_inc, tracker_inc],
    install: true,
    link_args: is_windows ? ['-Wl,--disable-stdcall-fixup', '-Wl,--enable-auto-import', '-Wl,--no-insert-timestamp', '-static-libgcc', '-static-libstdc++', '-lmsvcrt'] : [],
    win_subsystem: is_windows ? 'windows' : 'console',
)

srcs_sudoku_resources = [
    'Sudoku.rdef'
]

sudoku_resources = custom_target('sudoku resources',
    depends : [sudoku_app, rc_app],
    input : srcs_sudoku_resources,
    output : 'Sudoku.rsrc',
    command : [rc_app, '@INPUT@', '-o', '@OUTPUT@', '-I', '.'],
    build_by_default : true)

custom_target('sudoku plus resources',
    depends : sudoku_resources,
    input : sudoku_app,
    output : 'fake',
    command : [xres_app, '-o', '@INPUT@', sudoku_resources],
    build_by_default : true)