#!/bin/bash
# Test runner for BTestSuite-based tests using UnitTester

cd "$(dirname "$0")/builddir"

# Setup library paths
export LD_LIBRARY_PATH=$PWD/src/kits:$PWD/src/tools/cppunit:$LD_LIBRARY_PATH

# Create test addon directory if it doesn't exist
mkdir -p src/tests/lib

# Copy test addons to the expected location
cp -f src/tests/kits/storage/storagekittest.so src/tests/lib/
cp -f src/tests/kits/support/supportkittest.so src/tests/lib/

# Run UnitTester with absolute path
exec $PWD/src/tests/UnitTester "$@"
