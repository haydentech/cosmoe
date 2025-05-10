build: configure
	ninja -C builddir

configure:
	meson setup builddir --reconfigure

install: configure
	ninja -C builddir install

PHONY: build configure install