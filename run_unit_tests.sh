#!/bin/bash
# Test runner for BTestSuite-based tests using UnitTester

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

use_wine=0
if [ "${1:-}" = "--wine" ]; then
	use_wine=1
	shift
fi

if [ "${RUN_UNIT_TESTS_WINE:-0}" = "1" ]; then
	use_wine=1
fi

if [ $use_wine -eq 1 ]; then
	BUILD_DIR="$SCRIPT_DIR/builddir-windows"
	WIN_TEST_ROOT="$SCRIPT_DIR/win-test"
	COLLECT_SCRIPT="$SCRIPT_DIR/collect-windows-binaries.sh"

	if ! command -v wine >/dev/null 2>&1; then
		echo "Error: --wine requested but 'wine' is not installed"
		exit 1
	fi

	if [ ! -d "$BUILD_DIR" ]; then
		echo "Error: Windows build directory $BUILD_DIR not found"
		echo "Please build the Windows configuration first: make windows"
		exit 1
	fi

	CROSS_FILE="$SCRIPT_DIR/cross-mxe.ini"

	if ! command -v winepath >/dev/null 2>&1; then
		echo "Error: --wine requested but 'winepath' is not installed"
		exit 1
	fi

	if [ ! -x "$COLLECT_SCRIPT" ]; then
		echo "Error: Windows staging helper $COLLECT_SCRIPT not found or not executable"
		exit 1
	fi

	"$COLLECT_SCRIPT"
	mkdir -p "$WIN_TEST_ROOT/lib"
	find "$BUILD_DIR/src/tests/kits" -maxdepth 2 -type f -name '*kittest.dll' -exec cp -f {} "$WIN_TEST_ROOT/lib/" \;
	cd "$WIN_TEST_ROOT"

	BUILD_ROOT_WINE="$(winepath -w "$PWD")"
	WINEPATH_ENTRIES=("$BUILD_ROOT_WINE")

	if [ -f "$CROSS_FILE" ]; then
		MXE_SYSROOT="$(sed -n "s/^sys_root = '\(.*\)'/\1/p" "$CROSS_FILE" | head -n 1)"
		if [ -n "$MXE_SYSROOT" ] && [ -d "$MXE_SYSROOT/bin" ]; then
			WINEPATH_ENTRIES+=("$(winepath -w "$MXE_SYSROOT/bin")")
		fi
	fi

	# Wine needs the DLL lookup path to include the build root and MXE runtime DLLs.
	WINEPATH_JOINED="${WINEPATH_ENTRIES[0]}"
	for ((i = 1; i < ${#WINEPATH_ENTRIES[@]}; i++)); do
		WINEPATH_JOINED+=";${WINEPATH_ENTRIES[$i]}"
	 done
	export WINEPATH="$WINEPATH_JOINED${WINEPATH:+;$WINEPATH}"
	# Some runtimes consult PATH as well.
	export PATH="$PWD:$PATH"

	run_wine_test() {
		local test_path="$1"
		echo "Running under Wine: ${test_path#$PWD/}"
		wine "$test_path"
	}

	run_wine_unittester() {
		echo "Running under Wine: UnitTester.exe $*"
		wine "$PWD/UnitTester.exe" "$@"
	}

	if [ $# -gt 0 ]; then
		case "$1" in
			fs_attr_test|fs_attr_test.exe)
				exec wine "$PWD/fs_attr_test.exe"
				;;
			BPathTest|BPathTest.exe)
				exec wine "$PWD/BPathTest.exe"
				;;
			UnitTester|UnitTester.exe)
				shift
				exec wine "$PWD/UnitTester.exe" "$@"
				;;
			*)
				if [ -x "$PWD/$1" ]; then
					exec wine "$PWD/$1"
				elif [ -x "$PWD/$1.exe" ]; then
					exec wine "$PWD/$1.exe"
				else
					exec wine "$PWD/UnitTester.exe" "$@"
				fi
				;;
		esac
	fi

	echo "Running Wine-compatible Windows tests"
	if [ -x "$PWD/fs_attr_test.exe" ]; then
		run_wine_test "$PWD/fs_attr_test.exe"
	fi
	if [ -x "$PWD/BPathTest.exe" ]; then
		run_wine_test "$PWD/BPathTest.exe"
	fi
	run_wine_unittester "$@"
	exit $?
fi

cd "$SCRIPT_DIR/builddir"

# Setup library paths
export LD_LIBRARY_PATH=$PWD/src/kits:$PWD/src/tools/cppunit:$LD_LIBRARY_PATH

# Create test addon directory if it doesn't exist
mkdir -p src/tests/lib

# Copy test addons to the expected location - recurse to catch nested kit dirs
# like src/tests/kits/net/libnetapi.
find src/tests/kits -type f \( -name '*kittest.so' -o -name '*kittest.dylib' -o -name '*kittest.dll' \) -print0 | \
while IFS= read -r -d '' lib; do
	cp -f "$lib" src/tests/lib/
done

# If we have been requested to run BMessageRunner tests in separate processes
# use the dedicated runner script instead of UnitTester (enabled by env):
# Set RUN_BMESSAGERUNNER_SINGLE=1 to enable. This helps isolate threads/loopers.
if [ -n "${RUN_BMESSAGERUNNER_SINGLE:-}" ] && echo " $@ " | grep -q " App::BMessageRunner "; then
	echo "Running BMessageRunner tests in single-process mode via run_bmessagerunner_single_tests.sh"
	exec "$(dirname "$0")/src/tests/kits/app/bmessagerunner/run_bmessagerunner_single_tests.sh"
fi

# If UNIT_TEST_TIMEOUT is set, run UnitTester under the GNU timeout command.
# UNIT_TEST_TIMEOUT syntax should be an argument suitable for timeout, e.g. "60s"
# If the timeout command is not available, skip it.
if [ -n "${UNIT_TEST_TIMEOUT}" ]; then
	if command -v timeout >/dev/null 2>&1; then
		# Default to killing after 5 seconds if timeout triggers
		TIMEOUT_KILL_AFTER=${UNIT_TEST_KILL_AFTER:-5s}
		echo "Running UnitTester under timeout ${UNIT_TEST_TIMEOUT} (kill after ${TIMEOUT_KILL_AFTER})"
		exec timeout --preserve-status -k ${TIMEOUT_KILL_AFTER} ${UNIT_TEST_TIMEOUT} "$PWD/src/tests/UnitTester" "$@"
	else
		echo "Warning: UNIT_TEST_TIMEOUT set but 'timeout' command not found, running without timeout"
		exec "$PWD/src/tests/UnitTester" "$@"
	fi
else
	exec "$PWD/src/tests/UnitTester" "$@"
fi
