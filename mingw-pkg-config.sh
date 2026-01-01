#!/bin/bash
# Wrapper for pkg-config to use MSYS2 packages when cross-compiling

export PKG_CONFIG_PATH=/mnt/c/msys64/ucrt64/lib/pkgconfig
export PKG_CONFIG_SYSROOT_DIR=/mnt/c/msys64/ucrt64
export PKG_CONFIG_LIBDIR=/mnt/c/msys64/ucrt64/lib/pkgconfig

exec pkg-config "$@"
