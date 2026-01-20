#!/bin/bash

BUILD_DIR=~/cosmoe/builddir-windows

find $BUILD_DIR -name "*.exe" -exec cp {} ~/cosmoe/win-test/ \;
find $BUILD_DIR -name "*.dll" -exec cp {} ~/cosmoe/win-test/ \;
rm ~/cosmoe/win-test/libbe-bootstrap.dll