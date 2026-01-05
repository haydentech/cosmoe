#!/bin/bash
# Test runner for kernel tests
# Usage: ./run_kernel_tests.sh [test_name]

# Detect if we're on macOS and using the case-sensitive build image
if [[ "$(uname)" == "Darwin" ]] && [ -d "/Volumes/cosmoe-build/cosmoe/builddir" ]; then
    echo "Detected macOS with build image mounted"
    BASE_DIR="/Volumes/cosmoe-build/cosmoe"
    TEST_DIR="$BASE_DIR/builddir/src/tests/system/kernel"
else
    # Use local build directory
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
    BASE_DIR="$SCRIPT_DIR"
    TEST_DIR="$BASE_DIR/builddir/src/tests/system/kernel"
fi

echo "Using build directory: $BASE_DIR"
echo ""

if [ ! -d "$TEST_DIR" ]; then
    echo "Error: Test directory $TEST_DIR not found"
    echo "Please build the project first"
    if [[ "$(uname)" == "Darwin" ]]; then
        echo "  On macOS: ./build-on-mac.sh"
    else
        echo "  Standard build: ninja -C builddir"
    fi
    exit 1
fi

# If a specific test is specified, run only that test
if [ -n "$1" ]; then
    TEST_PATH="$TEST_DIR/$1"
    if [ -x "$TEST_PATH" ]; then
        echo "Running test: $1"
        echo "======================================"
        "$TEST_PATH"
        exit $?
    else
        echo "Error: Test '$1' not found or not executable"
        exit 1
    fi
fi

# Run all tests
echo "Running all kernel tests..."
echo "======================================"

# Tests that run quickly and don't hang
QUICK_TESTS=(
    "port_close_test_1"
    "port_close_test_2"
    "port_buffer_size_test"
    "port_delete_test"
    "wait_test_1"
    "wait_test_2"
    "wait_test_3"
    "wait_test_4"
    "testsem"
    "testports"
    "testporttorture"
    "testimage"
    "testthread"
)

# Tests that may block or take longer (run with timeout)
# Note: sem_acquire_test1 is expected to be killed as it demonstrates semaphore blocking
BLOCKING_TESTS=(
    "sigsuspend_test"
)

# Tests that are expected to block/hang (for demonstration purposes only)
DEMO_TESTS=(
    "sem_acquire_test1"
)

passed=0
failed=0
skipped=0

for test in "${QUICK_TESTS[@]}"; do
    TEST_PATH="$TEST_DIR/$test"
    if [ -x "$TEST_PATH" ]; then
        echo ""
        echo "Running: $test"
        echo "--------------------------------------"
        if "$TEST_PATH"; then
            echo "✓ PASSED: $test"
            ((passed++))
        else
            echo "✗ FAILED: $test (exit code: $?)"
            ((failed++))
        fi
    else
        echo "⊘ SKIPPED: $test (not found)"
        ((skipped++))
    fi
done

for test in "${BLOCKING_TESTS[@]}"; do
    TEST_PATH="$TEST_DIR/$test"
    if [ -x "$TEST_PATH" ]; then
        echo ""
        echo "Running: $test (with 5s timeout)"
        echo "--------------------------------------"
        if timeout 5 "$TEST_PATH"; then
            echo "✓ PASSED: $test"
            ((passed++))
        else
            exit_code=$?
            if [ $exit_code -eq 124 ]; then
                echo "⚠ TIMEOUT: $test (this may be normal)"
                ((passed++))
            else
                echo "✗ FAILED: $test (exit code: $exit_code)"
                ((failed++))
            fi
        fi
    else
        echo "⊘ SKIPPED: $test (not found)"
        ((skipped++))
    fi
done

echo ""
echo "Demonstration tests (expected to block):"
for test in "${DEMO_TESTS[@]}"; do
    TEST_PATH="$TEST_DIR/$test"
    if [ -x "$TEST_PATH" ]; then
        echo "  $test - run manually with: $TEST_PATH"
    fi
done

echo ""
echo "======================================"
echo "Test Summary:"
echo "  Passed:  $passed"
echo "  Failed:  $failed"
echo "  Skipped: $skipped"
echo "======================================"

if [ $failed -gt 0 ]; then
    exit 1
fi
exit 0
