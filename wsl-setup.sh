#!/bin/bash
# Setup script for cross-compiling cosmoe from WSL to Windows
# Run this in WSL after copying the cosmoe directory

set -e

echo "=== Cosmoe Cross-Compilation Setup for WSL ==="
echo

# Check if we're in WSL
if ! grep -qi microsoft /proc/version; then
    echo "ERROR: This script must be run in WSL, not native Linux or Windows"
    exit 1
fi

# Check if mingw-w64 is installed
echo "1. Checking for mingw-w64 toolchain..."
if ! command -v x86_64-w64-mingw32-gcc &> /dev/null; then
    echo "   mingw-w64 not found. Installing..."
    sudo apt update
    sudo apt install -y mingw-w64 g++-mingw-w64-x86-64 pkg-config
else
    echo "   ✓ mingw-w64 found: $(x86_64-w64-mingw32-gcc --version | head -1)"
fi

# Check if MSYS2 directory is accessible
echo
echo "2. Checking MSYS2 installation..."
if [ ! -d "/mnt/c/msys64/ucrt64" ]; then
    echo "   ERROR: MSYS2 not found at /mnt/c/msys64/ucrt64"
    echo "   Please ensure MSYS2 is installed on Windows at C:\\msys64"
    exit 1
else
    echo "   ✓ MSYS2 found at /mnt/c/msys64/ucrt64"
fi

# Test pkg-config with MSYS2 packages
echo
echo "3. Testing pkg-config with MSYS2 packages..."
export PKG_CONFIG_PATH="/mnt/c/msys64/ucrt64/lib/pkgconfig"
export PKG_CONFIG_SYSROOT_DIR="/mnt/c/msys64/ucrt64"

if x86_64-w64-mingw32-pkg-config --exists cairo 2>/dev/null; then
    echo "   ✓ cairo found: $(x86_64-w64-mingw32-pkg-config --modversion cairo)"
else
    echo "   ⚠ WARNING: cairo not found via pkg-config"
    echo "   This might work anyway with direct paths in cross-file"
fi

if x86_64-w64-mingw32-pkg-config --exists pango 2>/dev/null; then
    echo "   ✓ pango found: $(x86_64-w64-mingw32-pkg-config --modversion pango)"
fi

# Check if meson is installed
echo
echo "4. Checking for Meson build system..."
if ! command -v meson &> /dev/null; then
    echo "   Meson not found. Installing..."
    sudo apt install -y meson ninja-build
else
    echo "   ✓ Meson found: $(meson --version)"
fi

# Check ninja
if ! command -v ninja &> /dev/null; then
    echo "   Installing ninja..."
    sudo apt install -y ninja-build
else
    echo "   ✓ Ninja found: $(ninja --version)"
fi

echo
echo "=== Setup Complete ==="
echo
echo "Next steps:"
echo "1. cd to your cosmoe directory (likely /mnt/c/git/cosmoe)"
echo "2. Run: export PKG_CONFIG_PATH=/mnt/c/msys64/ucrt64/lib/pkgconfig"
echo "3. Run: meson setup --cross-file windows-cross.txt builddir-wsl"
echo "4. Run: ninja -C builddir-wsl"
