#!/bin/bash

set -euo pipefail

# Script to copy built executables and DLLs to Windows for testing

BUILDDIR="${1:-builddir-windows}"
DEST="${2:-/mnt/c/Cosmoe}"
MXE_BIN="/home/hayden/mxe/usr/x86_64-w64-mingw32.shared/bin"
MSYS2_BIN="/mnt/c/msys64/ucrt64/bin"

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

# Copy runtime dependencies
echo ""
echo "Copying runtime dependencies (prefer MXE to avoid ABI mismatch)..."
RUNTIME_LIBS=(
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
    "libbz2.dll"
    "libexpat-1.dll"
    "libpixman-1-0.dll"
    "libbrotlidec.dll"
    "libbrotlicommon.dll"
    "libfribidi-0.dll"
    "libgmodule-2.0-0.dll"
)

# These must come from the same MXE toolchain as the built binaries.
MXE_REQUIRED_LIBS=(
    "libgcc_s_seh-1.dll"
    "libstdc++-6.dll"
    "libwinpthread-1.dll"
)

# Copy the core GCC runtime from MXE only.
echo ""
echo "Copying required MXE runtime DLLs..."
for lib in "${MXE_REQUIRED_LIBS[@]}"; do
    if [ -f "$MXE_BIN/$lib" ]; then
        cp -v "$MXE_BIN/$lib" "$DEST/"
    else
        echo "ERROR: required MXE runtime missing: $MXE_BIN/$lib"
        echo "Aborting to avoid mixing incompatible MSYS2 runtime DLLs."
        exit 1
    fi
done

for lib in "${RUNTIME_LIBS[@]}"; do
    if [ -f "$MXE_BIN/$lib" ]; then
        cp -v "$MXE_BIN/$lib" "$DEST/"
    else
        echo "Warning: $lib not found in $MXE_BIN"
    fi
done

# ICU DLL names are versioned and change frequently. Copy whatever versions exist.
echo ""
echo "Copying ICU runtime DLLs (version-agnostic)..."
for pattern in icuuc*.dll icuin*.dll icudt*.dll; do
    found=0
    for src in "$MXE_BIN"/$pattern; do
        if [ -f "$src" ]; then
            cp -v "$src" "$DEST/"
            found=1
        fi
    done
    if [ $found -eq 0 ]; then
        echo "Warning: no matches for $pattern"
    fi
done

echo ""
echo "Done! Files copied to $DEST"

verify_copied_binary() {
    local src="$1"
    local dst="$2"
    if [ ! -f "$src" ]; then
        echo "ERROR: source file missing for verification: $src"
        exit 1
    fi
    if [ ! -f "$dst" ]; then
        echo "ERROR: destination file missing after copy: $dst"
        exit 1
    fi

    local src_hash dst_hash
    src_hash=$(sha256sum "$src" | awk '{print $1}')
    dst_hash=$(sha256sum "$dst" | awk '{print $1}')
    if [ "$src_hash" != "$dst_hash" ]; then
        echo "ERROR: verification failed for $(basename "$dst")"
        echo "  source:      $src"
        echo "  destination: $dst"
        echo "  This usually means the destination file is locked by a running Windows process."
        echo "  Close all Cosmoe apps (and any process using C:\\Cosmoe DLLs), then run this script again."
        exit 1
    fi
}

# Verify key DLLs were truly replaced (not left stale due file locks).
verify_copied_binary "$BUILDDIR/src/backends/windows/libcosmoe-windows.dll" "$DEST/libcosmoe-windows.dll"
verify_copied_binary "$BUILDDIR/libbe.dll" "$DEST/libbe.dll"

echo "Verified updated binaries in $DEST"

cat > "$DEST/run-cosmoe-debug.cmd" << 'EOF'
@echo off
setlocal

if "%~1"=="" (
    echo Usage: run-cosmoe-debug.cmd ^<app.exe^>
    echo Example: run-cosmoe-debug.cmd AboutSystem.exe
    exit /b 2
)

set "APP=%~1"
set "BASE=%~dp0"
set "LOGDIR=%BASE%logs"
if not exist "%LOGDIR%" mkdir "%LOGDIR%"

set "COSMOE_DEBUG_LOG=1"
set "LOGFILE=%LOGDIR%\%~n1-console.log"

pushd "%BASE%"
echo [cosmoe-debug] Launching %APP%
echo [cosmoe-debug] Console log: %LOGFILE%

"%BASE%%APP%" 1>"%LOGFILE%" 2>&1
set "APP_EXIT=%ERRORLEVEL%"

echo ExitCode=%APP_EXIT%>>"%LOGFILE%"
echo [cosmoe-debug] ExitCode=%APP_EXIT%
echo [cosmoe-debug] Backend log (if created): %BASE%cosmoe_debug.log

popd
endlocal
EOF

cat > "$DEST/enable-cosmoe-debug.cmd" << 'EOF'
@echo off
setlocal
pushd "%~dp0"
type nul > cosmoe_debug.on
echo Created %CD%\cosmoe_debug.on
echo Explorer launches will now write backend logs to cosmoe_debug.log
popd
endlocal
EOF

cat > "$DEST/disable-cosmoe-debug.cmd" << 'EOF'
@echo off
setlocal
pushd "%~dp0"
if exist cosmoe_debug.on del /f /q cosmoe_debug.on
echo Removed %CD%\cosmoe_debug.on
popd
endlocal
EOF

echo "Created debug helpers:"
echo "  $DEST/run-cosmoe-debug.cmd"
echo "  $DEST/enable-cosmoe-debug.cmd"
echo "  $DEST/disable-cosmoe-debug.cmd"
echo "You can now run executables from Windows Explorer or cmd.exe"
echo ""
echo "Note: GUI apps need to be run from Windows (not WSL terminal)"
echo "Example: From Windows Explorer, navigate to destination folder and double-click an .exe"
