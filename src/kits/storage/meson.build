srcs_storage = [
    'AppFileInfo.cpp',
    'Directory.cpp',
    'DriverSettings.cpp',
    'Entry.cpp',
    'EntryList.cpp',
    # 'EntryOperationEngineBase.cpp',
    # 'FdIO.cpp',
    'File.cpp',
    # 'FileDescriptorIO.cpp',
    # 'FileIO.cpp',
    'FindDirectory.cpp',
    'kernel_interface.cpp',  # Common platform-agnostic code
    'MergedDirectory.cpp',
    'Mime.cpp',
    'MimeType.cpp',
    'Node.cpp',
    'NodeInfo.cpp',
    'NodeMonitor.cpp',
    # 'NodeMonitorHandler.cpp',
    'OffsetFile.cpp',
    'Path.cpp',
    'PathFinder.cpp',
    'PathMonitor.cpp',
    'Query.cpp',
    'QueryPredicate.cpp',
    'ResourceFile.cpp',
    'ResourceItem.cpp',
    'ResourcesContainer.cpp',
    'Resources.cpp',
    'ResourceStrings.cpp',
    'Statable.cpp',
    'storage_support.cpp',
    'SymLink.cpp',
    'Volume.cpp',
    'VolumeRoster.cpp',
	'mime/database_support.cpp',
	'mime/DatabaseLocation.cpp',
]

# Add platform-specific kernel interface implementation
if is_windows
    srcs_storage += ['kernel_interface.WIN.cpp']
else
    srcs_storage += ['kernel_interface.POSIX.cpp']
endif

srcs_storage = files(srcs_storage)