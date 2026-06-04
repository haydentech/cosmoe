app_name = 'Terminal'
linux_desktop_id = 'x-vnd.Haiku-Terminal'
linux_desktop_categories = 'System;TerminalEmulator;'

# Terminal app only makes sense on Unix-like systems with PTY support

app_resource_defs = files(
    'Terminal.rdef',
    'XColors.rdef'
)

app_srcs = files(
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
)

# Terminal app only makes sense on Unix-like systems with PTY support.
app_enabled = not is_windows
app_dependencies = [dep_libbe, dep_libtracker]
app_include_directories = common_inc
