#!/bin/bash

# Resolve the repository root as the directory where this script lives.
COSMOE_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR=$COSMOE_DIR/builddir-windows

mkdir -p "$COSMOE_DIR/win-test"

find $BUILD_DIR -name "*.exe" -exec cp {} $COSMOE_DIR/win-test/ \;
find $BUILD_DIR -name "*.dll" ! -name "libbe-bootstrap.dll" -exec cp {} $COSMOE_DIR/win-test/ \;
