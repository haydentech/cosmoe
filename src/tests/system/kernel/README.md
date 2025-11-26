# Kernel Tests

This directory contains kernel-level tests for the Cosmoe/Haiku compatibility layer.

## Building Tests

The tests are integrated into the meson build system. Simply build the project:

```bash
cd /home/billh/git/cow
ninja -C builddir
```

The test executables will be built in `builddir/src/tests/system/kernel/`.

## Running Tests

### Run All Tests

Use the provided test runner script:

```bash
./run_kernel_tests.sh
```

### Run a Specific Test

```bash
./run_kernel_tests.sh port_close_test_1
```

Or run the executable directly:

```bash
builddir/src/tests/system/kernel/port_close_test_1
```

## Test Categories

### Port Tests
Tests for message ports (inter-process communication):
- `port_close_test_1`, `port_close_test_2` - Port closing behavior
- `port_buffer_size_test` - Port buffer size queries
- `port_delete_test` - Port deletion
- `port_wakeup_test_1` through `port_wakeup_test_9` - Port wakeup scenarios

### Semaphore Tests
- `sem_acquire_test1` - Semaphore acquisition (demonstration, blocks indefinitely)

### Wait/Process Tests
- `wait_test_1`, `wait_test_2`, `wait_test_3`, `wait_test_4` - Process wait() behavior
- `sigsuspend_test` - Signal suspension

### Other Tests
- `advisory_locking_test` - File locking
- `cow_bug113_test` - Copy-on-write bug reproduction
- `fibo_exec`, `fibo_fork`, `fibo_load_image` - Process creation tests
- `lock_node_test` - Node locking
- `mtrr_power_test` - MTRR (Memory Type Range Register) tests
- `page_fault_cache_merge_test` - Page fault handling
- `path_resolution_test` - Path resolution
- `select_check` - select() system call
- `set_area_protection_test1` - Memory area protection

## Tests Not Built

The following tests are excluded from the build due to portability issues:

- `select_close_test` - Pointer to int cast issues on 64-bit systems
- `sigint_bug113_test` - Uses Haiku-specific SIGKILLTHR signal
- `spinlock_contention` - Missing header file (spinlock_contention.h)
- `syscall_restart_test` - Uses Haiku-specific sa_userdata and switch_sem
- `syscall_time` - Portability issues
- `transfer_area_test` - Uses Haiku-specific _kern_transfer_area
- `wait_for_objects_test` - Linking issues
- `yield_test` - Uses Haiku-specific _kern_thread_yield

## Installing Tests

To install the tests to `/usr/local/bin/tests/kernel`:

```bash
sudo ninja -C builddir install
```

Then run from the installed location:

```bash
/usr/local/bin/tests/kernel/port_close_test_1
```

## Test Results

All currently building tests should pass. Some tests may:
- Print warnings about format specifiers (these are cosmetic)
- Block indefinitely if they're demonstration tests (like `sem_acquire_test1`)
- Require specific system conditions to demonstrate their behavior

## Adding New Tests

To add a new test:

1. Add the source file to `src/tests/system/kernel/`
2. Add the test name to the appropriate list in `src/tests/system/kernel/meson.build`
3. Rebuild: `ninja -C builddir`
4. Update `run_kernel_tests.sh` if needed

## Troubleshooting

### Test won't build
Check if it uses Haiku-specific APIs that aren't available on Linux. You may need to exclude it from the meson.build file.

### Test hangs
Some tests (like `sem_acquire_test1`) are designed to demonstrate blocking behavior. Use Ctrl+C to exit or run with a timeout:

```bash
timeout 5 builddir/src/tests/system/kernel/sem_acquire_test1
```

### Test fails
Check the test output for error messages. Many tests print diagnostic information that explains what they're testing and what the expected behavior is.
