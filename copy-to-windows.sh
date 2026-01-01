#!/bin/bash

# Script to copy built executables and DLLs to Windows for testing

BUILDDIR="${1:-builddir-wsl}"
DEST="/mnt/c/Cosmoe"

if [ ! -d "$BUILDDIR" ]; then
    echo "Error: Build directory '$BUILDDIR' not found"
    echo "Usage: $0 [builddir]"
    exit 1
fi

# Create destination directory if it doesn't exist
mkdir -p "$DEST"

echo "Copying executables and DLLs from $BUILDDIR to $DEST..."

# Copy all DLLs (libraries needed by executables)
echo "Copying DLLs..."
find "$BUILDDIR" -name "*.dll" -exec cp -v {} "$DEST/" \;

# Copy all executables
echo ""
echo "Copying executables..."
find "$BUILDDIR" -name "*.exe" -exec cp -v {} "$DEST/" \;

# Copy MSYS2 runtime dependencies
echo ""
echo "Copying MSYS2 runtime dependencies..."
MSYS2_LIBS=(
    "libgcc_s_seh-1.dll"
    "libstdc++-6.dll"
    "libwinpthread-1.dll"
    "libcairo-2.dll"
    "libglib-2.0-0.dll"
    "libgobject-2.0-0.dll"
    "libgio-2.0-0.dll"
    "libpango-1.0-0.dll"
    "libpangocairo-1.0-0.dll"
    "libpangoft2-1.0-0.dll"
    "libpangowin32-1.0-0.dll"
    "libharfbuzz-0.dll"
    "libfreetype-6.dll"
    "libfontconfig-1.dll"
    "libpng16-16.dll"
    "zlib1.dll"
    "libintl-8.dll"
    "libiconv-2.dll"
    "libpcre2-8-0.dll"
    "libffi-8.dll"
    "libbz2-1.dll"
    "libexpat-1.dll"
    "libpixman-1-0.dll"
    "libgraphite2.dll"
    "libbrotlidec.dll"
    "libbrotlicommon.dll"
    "libfribidi-0.dll"
    "libicuuc78.dll"
    "libicuin78.dll"
    "libicudt78.dll"
    "libthai-0.dll"
    "libdatrie-1.dll"
    "libgmodule-2.0-0.dll"
)

# Copy Universal C Runtime (UCRT) DLLs - required by MinGW-compiled programs
echo ""
echo "Copying Universal C Runtime (UCRT) DLLs..."

# Try to copy ucrtbase.dll from Windows System32
if [ -f "/mnt/c/Windows/System32/ucrtbase.dll" ]; then
    cp -v "/mnt/c/Windows/System32/ucrtbase.dll" "$DEST/"
else
    echo "Warning: ucrtbase.dll not found in C:\\Windows\\System32"
fi

# Check for UCRT API DLLs - these are usually in System32 if properly installed
UCRT_API_DLLS_FOUND=0
for dll in api-ms-win-crt-runtime-l1-1-0.dll api-ms-win-crt-stdio-l1-1-0.dll; do
    if [ -f "/mnt/c/Windows/System32/$dll" ]; then
        UCRT_API_DLLS_FOUND=1
        break
    fi
done

if [ $UCRT_API_DLLS_FOUND -eq 0 ]; then
    echo ""
    echo "WARNING: Universal C Runtime (UCRT) API DLLs not found!"
    echo "Your system is missing api-ms-win-crt-*.dll files."
    echo ""
    echo "To fix this, install the Visual C++ Redistributable:"
    echo "  Download from: https://aka.ms/vs/17/release/vc_redist.x64.exe"
    echo "  Or run from PowerShell:"
    echo "    winget install Microsoft.VCRedist.2015+.x64"
    echo ""
    echo "Without these DLLs, MinGW-compiled programs will fail to launch."
    echo ""
fi

for lib in "${MSYS2_LIBS[@]}"; do
    if [ -f "/mnt/c/msys64/ucrt64/bin/$lib" ]; then
        cp -v "/mnt/c/msys64/ucrt64/bin/$lib" "$DEST/"
    else
        echo "Warning: $lib not found in /mnt/c/msys64/ucrt64/bin/"
    fi
done

echo ""
echo "Done! Files copied to C:\\Cosmoe"
echo "You can now run executables from Windows Explorer or cmd.exe"
echo ""
echo "Note: GUI apps need to be run from Windows (not WSL terminal)"
echo "Example: From Windows Explorer, navigate to C:\\Cosmoe and double-click an .exe"
