#!/bin/bash

# Change this to your local path
COSMOE_DIR=~/git/cosmoe
BUILD_DIR=$COSMOE_DIR/builddir-windows

find $BUILD_DIR -name "*.exe" -exec cp {} $COSMOE_DIR/win-test/ \;
find $BUILD_DIR -name "*.dll" -exec cp {} $COSMOE_DIR/win-test/ \;
rm $COSMOE_DIR/win-test/libbe-bootstrap.dll
