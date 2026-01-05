#!/bin/bash
# build-on-image.sh
# Creates a case-sensitive disk image for building cosmoe on macOS
# This is useful because macOS typically uses case-insensitive filesystems

set -e  # Exit on error

# Save original directory to return to it at the end
ORIGINAL_DIR="$(pwd)"

# Configuration
IMAGE_NAME="cosmoe-build"
IMAGE_SIZE="10g"  # 10GB sparse image (will only use space as needed)
MOUNT_POINT="/Volumes/${IMAGE_NAME}"
IMAGE_PATH="${HOME}/${IMAGE_NAME}.sparseimage"
SOURCE_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${MOUNT_POINT}/cosmoe"

echo "=== Cosmoe Build on Case-Sensitive Image ==="
echo "Image: ${IMAGE_PATH}"
echo "Mount: ${MOUNT_POINT}"
echo "Source: ${SOURCE_DIR}"
echo ""

# Check if image exists, create if needed
if [ ! -f "${IMAGE_PATH}" ]; then
    echo "Creating sparse case-sensitive disk image..."
    hdiutil create \
        -size "${IMAGE_SIZE}" \
        -type SPARSE \
        -fs "Case-sensitive APFS" \
        -volname "${IMAGE_NAME}" \
        "${IMAGE_PATH}"
    echo "✓ Image created"
else
    echo "✓ Image already exists"
fi

# Check if already mounted
if [ -d "${MOUNT_POINT}" ]; then
    echo "✓ Image already mounted at ${MOUNT_POINT}"
else
    echo "Mounting image..."
    hdiutil attach "${IMAGE_PATH}"
    echo "✓ Image mounted"
fi

# Ensure mount point exists
if [ ! -d "${MOUNT_POINT}" ]; then
    echo "ERROR: Mount point ${MOUNT_POINT} does not exist after mounting"
    exit 1
fi

# Handle clean command
if [ "$1" = "clean" ]; then
    echo ""
    echo "Cleaning build directory..."
    if [ -d "${BUILD_DIR}/builddir" ]; then
        rm -rf "${BUILD_DIR}/builddir"
        echo "✓ Build directory removed"
    else
        echo "✓ Build directory doesn't exist"
    fi
    echo ""
    echo "=== Clean Complete ==="
    cd "${ORIGINAL_DIR}"
    exit 0
fi

# Handle distclean command
if [ "$1" = "distclean" ]; then
    echo ""
    echo "Removing build image..."
    
    # Unmount if mounted
    if [ -d "${MOUNT_POINT}" ]; then
        echo "Unmounting ${MOUNT_POINT}..."
        hdiutil detach "${MOUNT_POINT}" 2>/dev/null || true
        echo "✓ Image unmounted"
    fi
    
    # Delete the image file
    if [ -f "${IMAGE_PATH}" ]; then
        echo "Deleting ${IMAGE_PATH}..."
        rm -f "${IMAGE_PATH}"
        echo "✓ Image deleted"
    else
        echo "✓ Image doesn't exist"
    fi
    
    echo ""
    echo "=== Distclean Complete ==="
    cd "${ORIGINAL_DIR}"
    exit 0
fi

# Sync source to build image
echo ""
echo "Syncing source tree to build image..."
rsync -av --delete \
    --exclude='.git' \
    --exclude='builddir' \
    --exclude='*.o' \
    --exclude='*.dylib' \
    --exclude='*.a' \
    --exclude='*.rsrc' \
    --exclude='__pycache__' \
    --exclude='.DS_Store' \
    --exclude='Makefile' \
    --exclude='build-on-mac.sh' \
    "${SOURCE_DIR}/" "${BUILD_DIR}/"
echo "✓ Source synced"

# Build
echo ""
echo "Building Cosmoe..."
cd "${BUILD_DIR}"

# Set up ICU path for macOS
export PKG_CONFIG_PATH="/opt/homebrew/opt/icu4c/lib/pkgconfig:${PKG_CONFIG_PATH}"

# Configure if build.ninja doesn't exist
if [ ! -f "builddir/build.ninja" ]; then
    echo "Configuring build..."
    meson setup builddir
else
    echo "✓ Build already configured"
fi

# Build
echo "Compiling..."
if [ "$1" = "install" ]; then
    echo "Installing..."
    sudo ninja -C builddir install
else
    ninja -C builddir
fi

echo ""
echo "Build directory: ${BUILD_DIR}/builddir"
if [ "$1" = "install" ]; then
    echo "=== Build and Install Complete ==="
else
    echo "=== Build Complete ==="
    echo "To install:                  $0 install"
fi
echo "To manually unmount image:   hdiutil detach ${MOUNT_POINT}"
echo "To remove build objects:     $0 clean"
echo "To unmount and remove image: $0 distclean"
echo ""

# Return to original directory
cd "${ORIGINAL_DIR}"
