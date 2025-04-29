srcs_terminal = [
    'ActiveProcessInfo.cpp',
    'AppearPrefView.cpp',
    'Arguments.cpp',
    'BasicTerminalBuffer.cpp',
    'Colors.cpp',
    'FindWindow.cpp',
    'Globals.cpp',
    'HistoryBuffer.cpp',
    'HyperLink.cpp',
    'InlineInput.cpp',
    'PatternEvaluator.cpp',
    'PrefHandler.cpp',
    'PrefWindow.cpp',
    'SetTitleDialog.cpp',
    'Shell.cpp',
    'ShellInfo.cpp',
    'ShellParameters.cpp',
    'SmartTabView.cpp',
    'TermApp.cpp',
    'TermConst.cpp',
    'TerminalBuffer.cpp',
    'TerminalCharClassifier.cpp',
    'TerminalRoster.cpp',
    'TermParse.cpp',
    'TermScrollView.cpp',
    'TermView.cpp',
    'TermViewHighlight.cpp',
    'TermViewStates.cpp',
    'TermWindow.cpp',
    'ThemeView.cpp',
    'ThemeWindow.cpp',
    'TitlePlaceholderMapper.cpp',
    'VTKeyTbl.c',
    'VTPrsTbl.c',
]

terminal_app = executable(
    'Terminal',
    srcs_terminal,
    dependencies: dep_libbe,
    include_directories: common_inc,
    install: true,
)

srcs_terminal_resources = [
    'Terminal.rdef',
    'XColors.rdef'
]

terminal_resources = custom_target('terminal resources',
    depends : [terminal_app, rc_app],
    input : srcs_terminal_resources,
    output : 'Terminal.rsrc',
    command : [rc_app, '@INPUT@', '-o', '@OUTPUT@', '-I', '.'],
    build_by_default : true)

custom_target('terminal plus resources',
    depends : terminal_resources,
    input : terminal_app,
    output : 'fake',
    command : [xres_app, '-o', '@INPUT@', terminal_resources],
    build_by_default : true)