app_name = 'ResEdit'

app_srcs = [
    'App.cpp',
    'BitmapView.cpp',
    'ImageEditor.cpp',
    'MiscEditors.cpp',
    'NumberEditors.cpp',
    'PreviewColumn.cpp',
    'ResFields.cpp',
    'ResListView.cpp',
    'ResourceData.cpp',
    'ResourceRoster.cpp',
    'ResView.cpp',
    'ResWindow.cpp',
]


be_app = executable(
    app_name,
    app_srcs,
    dependencies: dep_libbe,
    include_directories: common_inc,
    install: true,
)
