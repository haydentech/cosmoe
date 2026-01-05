ifeq ($(shell id -u),0)
$(error Do not run this Makefile as root. Meson will request elevated privileges if needed.)
endif

# Detect operating system
UNAME_S := $(shell uname -s)

# Detect if we're on macOS
ifeq ($(UNAME_S),Darwin)
    USE_BUILD_SCRIPT := yes
    BUILD_SCRIPT := ./build-on-mac.sh
else
    USE_BUILD_SCRIPT := no
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

ifeq ($(USE_BUILD_SCRIPT),yes)
build:
	$(BUILD_SCRIPT)

configure:
	@echo "Configuration is handled automatically by $(BUILD_SCRIPT)"

install:
	$(BUILD_SCRIPT) install

clean:
	$(BUILD_SCRIPT) clean

distclean:
	$(BUILD_SCRIPT) distclean
else
build: configure
	ninja -C $(BUILDDIR)

configure:
	$(PKG_CONFIG_SETUP) meson setup $(CROSS_FILE) $(BUILDDIR) --reconfigure

install: configure
	ninja -C $(BUILDDIR) install

clean:
	rm -rf $(BUILDDIR)

distclean: clean
	@echo "Distclean complete (builddir removed)"
endif

.PHONY: build configure install clean distclean