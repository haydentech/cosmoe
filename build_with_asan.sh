#!/bin/bash
# Script to rebuild with AddressSanitizer for debugging stack/heap corruption
# Build tools will have leak detection suppressed

set -e

echo "=== Cleaning build directory ==="
rm -rf builddir
mkdir -p builddir

echo ""
echo "=== Configuring with AddressSanitizer ==="
# Configure with ASAN enabled for everything
meson setup builddir \
  -Db_sanitize=address \
  -Db_lundef=false \
  -Dc_args="-fno-omit-frame-pointer -g -O1" \
  -Dcpp_args="-fno-omit-frame-pointer -g -O1"

echo ""
echo "=== Building ==="
ninja -C builddir

echo ""
echo "=== Build complete ==="
echo ""
echo "Now run your application normally. ASAN will:"
echo "  - Detect buffer overflows"
echo "  - Detect use-after-free"
echo "  - Detect stack corruption"
echo "  - Show exact line where corruption happens"
echo ""
echo "Example:"
echo "  cd builddir"
echo "  ./src/apps/gui-torture/gui-torture"
echo ""
echo "To go back to a non-ASAN build, delete your builddir and rebuild"

