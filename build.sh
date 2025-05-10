#!/bin/sh

meson setup builddir --reconfigure
ninja -C builddir
