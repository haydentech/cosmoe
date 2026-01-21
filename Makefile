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
# On Linux, however, you must explicitly use "make windows" to cross build for Windows
ifeq ($(shell test -d /mnt/c && echo yes),yes)
    BUILDDIR := builddir-windows
    CROSS_FILE := --cross-file cross-mxe.ini
else
    BUILDDIR := builddir
    CROSS_FILE :=
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

# Windows cross-compilation using MXE on Linux or WSL
windows:
	@echo "Configuring for Windows cross-compilation using MXE..."
	meson setup --cross-file cross-mxe.ini $(BUILDDIR)-windows --reconfigure
	@echo "Building Windows binaries..."
	ninja -C $(BUILDDIR)-windows
	@echo ""
	@echo "Windows build complete! Binaries are in $(BUILDDIR)-windows/"

windows-clean:
	rm -rf $(BUILDDIR)-windows
	@echo "Windows build directory removed"

.PHONY: build configure install clean distclean windows windows-clean