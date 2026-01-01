ifeq ($(shell id -u),0)
$(error Do not run this Makefile as root. Meson will request elevated privileges if needed.)
endif

# Detect if we're cross-compiling for Windows from WSL
# Check if /mnt/c exists (WSL) and if windows-cross.txt exists
ifeq ($(shell test -d /mnt/c && test -f windows-cross.txt && echo yes),yes)
    BUILDDIR := builddir-wsl
    CROSS_FILE := --cross-file windows-cross.txt
    PKG_CONFIG_SETUP := PKG_CONFIG_PATH=/mnt/c/msys64/ucrt64/lib/pkgconfig PKG_CONFIG_SYSROOT_DIR=/mnt/c/msys64/ucrt64
else
    BUILDDIR := builddir
    CROSS_FILE :=
    PKG_CONFIG_SETUP :=
endif

build: configure
	ninja -C $(BUILDDIR)

configure:
	$(PKG_CONFIG_SETUP) meson setup $(CROSS_FILE) $(BUILDDIR) --reconfigure

install: configure
	ninja -C $(BUILDDIR) install

clean:
	rm -rf $(BUILDDIR)

.PHONY: build configure install clean