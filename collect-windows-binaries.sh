#!/bin/bash

BUILD_DIR=~/git/cow/builddir-mxe

find $BUILD_DIR -name "*.exe" -exec cp {} ~/git/cow/wine-test/ \;
find $BUILD_DIR -name "*.dll" -exec cp {} ~/git/cow/wine-test/ \;
rm ~/git/cow/wine-test/libbe-bootstrap.dll