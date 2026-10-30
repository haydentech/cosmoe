srcs_support = [
    'Archivable.cpp',
    'ArchivingManagers.cpp',
	'Base64.cpp',
    'Beep.cpp',
    'BlockCache.cpp',
    'BufferedDataIO.cpp',
    'BufferIO.cpp',
    'ByteOrder.cpp',
	'CompressionAlgorithm.cpp',
    'DataIO.cpp',
    'DateTime.cpp',
    'Flattenable.cpp',
    'Job.cpp',
    'JobQueue.cpp',
    'List.cpp',
    'Locker.cpp',
    'PointerList.cpp',
    'Referenceable.cpp',
    'StopWatch.cpp',
    'String.cpp',
    'StringList.cpp',
	'Url.cpp',
    'Uuid.cpp',
	'ZlibCompressionAlgorithm.cpp'
]

# Add Windows-specific helper files
if is_windows
    srcs_support += ['string_helper.cpp']
endif

srcs_support = files(srcs_support)

# The build-machine copy of the kit follows the build machine, not the target.
srcs_support_build = srcs_support
if build_machine.system() == 'windows' and not is_windows
    srcs_support_build += files('string_helper.cpp')
endif
