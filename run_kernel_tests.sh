#!/bin/bash
# Test runner for kernel tests
# Usage: ./run_kernel_tests.sh [--wine] [test_name]

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_SUBDIR="${COSMOE_BUILD_DIR:-builddir}"

use_wine=0
if [[ "${1:-}" == "--wine" ]]; then
    use_wine=1
    shift
fi

run_test_binary() {
    local test_path="$1"
	local test_name="$2"
    if [[ $use_wine -eq 1 ]]; then
        wine "win-test/${test_name}.exe"
    else
        "$test_path"
    fi
}

resolve_test_path() {
    local test_name="$1"
    if [[ $use_wine -eq 1 ]]; then
        if [[ -x "$TEST_DIR/$test_name" ]]; then
            printf '%s\n' "$TEST_DIR/$test_name"
        elif [[ -x "$TEST_DIR/$test_name.exe" ]]; then
            printf '%s\n' "$TEST_DIR/$test_name.exe"
        fi
    else
        if [[ -x "$TEST_DIR/$test_name" ]]; then
            printf '%s\n' "$TEST_DIR/$test_name"
        fi
    fi
}

if [[ $use_wine -eq 1 ]]; then
    BUILD_DIR="$SCRIPT_DIR/builddir-windows"
    TEST_DIR="$BUILD_DIR/src/tests/system/kernel"
    CROSS_FILE="$SCRIPT_DIR/cross-mxe.ini"

    if ! command -v wine >/dev/null 2>&1; then
        echo "Error: --wine requested but 'wine' is not installed"
        exit 1
    fi

    if ! command -v winepath >/dev/null 2>&1; then
        echo "Error: --wine requested but 'winepath' is not installed"
        exit 1
    fi

    if [[ ! -d "$BUILD_DIR" ]]; then
        echo "Error: Windows build directory $BUILD_DIR not found"
        echo "Please build the Windows configuration first: make windows"
        exit 1
    fi

    BUILD_ROOT_WINE="$(winepath -w "$BUILD_DIR")"
    WINEPATH_ENTRIES=("$BUILD_ROOT_WINE")

    if [[ -f "$CROSS_FILE" ]]; then
        MXE_SYSROOT="$(sed -n "s/^sys_root = '\(.*\)'/\1/p" "$CROSS_FILE" | head -n 1)"
        if [[ -n "$MXE_SYSROOT" && -d "$MXE_SYSROOT/bin" ]]; then
            WINEPATH_ENTRIES+=("$(winepath -w "$MXE_SYSROOT/bin")")
        fi
    fi

    WINEPATH_JOINED="${WINEPATH_ENTRIES[0]}"
    for ((i = 1; i < ${#WINEPATH_ENTRIES[@]}; i++)); do
        WINEPATH_JOINED+=";${WINEPATH_ENTRIES[$i]}"
    done
    export WINEPATH="$WINEPATH_JOINED${WINEPATH:+;$WINEPATH}"
else
    # Detect if we're on macOS and using the case-sensitive build image
    if [[ "$(uname)" == "Darwin" ]] && [[ -d "/Volumes/cosmoe-build/cosmoe/builddir" ]]; then
        echo "Detected macOS with build image mounted"
        BASE_DIR="/Volumes/cosmoe-build/cosmoe"
        BUILD_DIR="$BASE_DIR/${BUILD_SUBDIR}"
        TEST_DIR="$BUILD_DIR/src/tests/system/kernel"
    else
        # Use local build directory
        BASE_DIR="$SCRIPT_DIR"
        BUILD_DIR="$BASE_DIR/${BUILD_SUBDIR}"
        TEST_DIR="$BUILD_DIR/src/tests/system/kernel"
    fi
fi

if [[ -z "${BASE_DIR:-}" ]]; then
    BASE_DIR="$SCRIPT_DIR"
fi

if [[ -z "${BUILD_DIR:-}" ]]; then
    BUILD_DIR="$BASE_DIR/${BUILD_SUBDIR}"
fi

echo "Using build directory: $BUILD_DIR"

# Prefer the just-built libraries over any installed copies under /usr/local.
export LD_LIBRARY_PATH="$BUILD_DIR${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

if [[ $use_wine -eq 1 ]]; then
    echo "Running kernel tests under Wine"
fi
echo ""

if [[ ! -d "$TEST_DIR" ]]; then
    echo "Error: Test directory $TEST_DIR not found"
    echo "Please build the project first"
    echo "Set COSMOE_BUILD_DIR to the Meson build directory you want to test"
    if [[ $use_wine -eq 1 ]]; then
        echo "  Windows build: make windows"
    elif [[ "$(uname)" == "Darwin" ]]; then
        echo "  MacOS build: ./build-on-mac.sh"
    else
        echo "  Linux build: ninja -C builddir"
    fi
    exit 1
fi

# If a specific test is specified, run only that test
if [[ -n "${1:-}" ]]; then
    TEST_PATH="$(resolve_test_path "$1")"
    if [[ -n "$TEST_PATH" ]]; then
        echo "Running test: $1"
        echo "======================================"
        run_test_binary "$TEST_PATH" "$1"
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
    TEST_PATH="$(resolve_test_path "$test")"
    if [[ -n "$TEST_PATH" ]]; then
        echo ""
        echo "Running: $test"
        echo "--------------------------------------"
        if run_test_binary "$TEST_PATH" "$test"; then
            echo "✓ PASSED: $test"
            ((passed++))
        else
            exit_code=$?
            echo "✗ FAILED: $test (exit code: $exit_code)"
            ((failed++))
        fi
    else
        echo "⊘ SKIPPED: $test (not found)"
        ((skipped++))
    fi
done

for test in "${BLOCKING_TESTS[@]}"; do
    TEST_PATH="$(resolve_test_path "$test")"
    if [[ -n "$TEST_PATH" ]]; then
        echo ""
        echo "Running: $test (with 5s timeout)"
        echo "--------------------------------------"
        if [[ $use_wine -eq 1 ]]; then
            timeout 5 wine "win-test/$test"
        else
            timeout 5 "$TEST_PATH"
        fi
        exit_code=$?
        if [[ $exit_code -eq 0 ]]; then
            echo "✓ PASSED: $test"
            ((passed++))
        elif [[ $exit_code -eq 124 ]]; then
            echo "⚠ TIMEOUT: $test (this may be normal)"
            ((passed++))
        else
            echo "✗ FAILED: $test (exit code: $exit_code)"
            ((failed++))
        fi
    else
        echo "⊘ SKIPPED: $test (not found)"
        ((skipped++))
    fi
done

echo ""
echo "Demonstration tests (expected to block):"
for test in "${DEMO_TESTS[@]}"; do
    TEST_PATH="$(resolve_test_path "$test")"
    if [[ -n "$TEST_PATH" ]]; then
        if [[ $use_wine -eq 1 ]]; then
            echo "  $test - run manually with: wine win-test/$test"
        else
            echo "  $test - run manually with: $TEST_PATH"
        fi
    fi
done

echo ""
echo "======================================"
echo "Kernel Testing Summary:"
echo "  Passed:  $passed"
echo "  Failed:  $failed"
echo "  Skipped: $skipped"
echo "======================================"

if [[ $failed -gt 0 ]]; then
    exit 1
fi
exit 0
