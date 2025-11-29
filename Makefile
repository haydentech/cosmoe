ifeq ($(shell id -u),0)
$(error Do not run this Makefile as root. Meson will request elevated privileges if needed.)
endif

build: configure
	ninja -C builddir

configure:
	meson setup builddir --reconfigure

install: configure
	ninja -C builddir install

.PHONY: build configure install