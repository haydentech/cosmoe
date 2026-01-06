#!/bin/bash
# Test runner for BTestSuite-based tests using UnitTester

cd "$(dirname "$0")/builddir"

# Setup library paths
export LD_LIBRARY_PATH=$PWD/src/kits:$PWD/src/tools/cppunit:$LD_LIBRARY_PATH

# Create test addon directory if it doesn't exist
mkdir -p src/tests/lib

# Copy test addons to the expected location - find all kit test libraries
# Handle both .so (Linux), .dylib (macOS), and .dll (Windows) extensions
for lib in src/tests/kits/*/*kittest.so src/tests/kits/*/*kittest.dylib src/tests/kits/*/*kittest.dll; do
	if [ -f "$lib" ]; then
		cp -f "$lib" src/tests/lib/
	fi
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
