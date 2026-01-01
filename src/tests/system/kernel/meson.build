# Kernel tests - standalone test executables

# Tests that use fork() - can't run on Windows
fork_tests = [
  'fibo_exec',
  'fibo_fork',
  'page_fault_cache_merge_test',
  'set_area_protection_test1',
  'spinlock_contention',
  'testthread',
  'wait_test_2',
  'wait_test_3',
  'wait_test_4',
]

# C++ tests that build and run on Linux
kernel_tests_cpp = [
  # 'advisory_locking_test',  # Uses fcntl/flock not available on Windows
  # 'cow_bug113_test',  # Uses fork/mmap which don't work the same on Windows
  'fibo_load_image',
  'lock_node_test',
  'path_resolution_test',
  'port_buffer_size_test',
  'port_close_test_1',
  'port_close_test_2',
  'port_delete_test',
  'port_wakeup_test_1',
  'port_wakeup_test_2',
  'port_wakeup_test_3',
  'port_wakeup_test_4',
  'port_wakeup_test_5',
  'port_wakeup_test_6',
  'port_wakeup_test_7',
  'port_wakeup_test_8',
  'port_wakeup_test_9',
  # 'select_check',  # Windows select() only works with sockets, not file descriptors
  # 'select_close_test',  # Pointer to int cast issues
  'sem_acquire_test1',
  # 'sigint_bug113_test',  # Uses SIGKILLTHR not available on Linux
  # 'sigsuspend_test',  # Uses sigsuspend not available on Windows
  # 'spinlock_contention',  # Missing spinlock_contention.h header
  # 'syscall_restart_test',  # Uses Haiku-specific sa_userdata and switch_sem
  # 'syscall_time',  # May have portability issues
  # 'transfer_area_test',  # Uses _kern_transfer_area
  # 'wait_for_objects_test',  # Linking issues
  # 'yield_test',  # Uses _kern_thread_yield
  'testsem',
  'testports',
  'testporttorture',
  'testimage',
]

# Add fork-dependent tests on non-Windows platforms
if not is_windows
  kernel_tests_cpp += fork_tests
endif

# C tests
kernel_tests_c = [
  'mtrr_power_test',
]

# C tests that use wait() - can't run on Windows
if not is_windows
  kernel_tests_c += ['wait_test_1']
endif

# Build C++ tests
foreach test_name : kernel_tests_cpp
  executable(
    test_name,
    test_name + '.cpp',
    include_directories: common_inc,
    dependencies: [dep_libroot],
    link_with: [lib_libbe],
    install: false
  )
endforeach

# Build C tests
foreach test_name : kernel_tests_c
  executable(
    test_name,
    test_name + '.c',
    include_directories: common_inc,
    dependencies: [dep_libroot],
    link_with: [lib_libbe],
    install: false
  )
endforeach
