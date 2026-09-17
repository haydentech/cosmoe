ifeq ($(shell id -u),0)
ifeq ($(filter deb,$(MAKECMDGOALS)),)
$(error Do not run this Makefile as root. Meson will request elevated privileges if needed.)
endif
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

# Debian package staging build. This deliberately uses a separate build directory
# and DESTDIR so no project files are installed into the live system.
DEB_BUILDDIR := builddir-deb
DEB_DESTDIR := $(CURDIR)/debian/tmp
DEB_MULTIARCH := $(shell dpkg-architecture -qDEB_HOST_MULTIARCH)


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
	rm -rf $(DEB_BUILDDIR) $(DEB_DESTDIR)

distclean: clean
	@echo "Distclean complete (builddir removed)"
endif

ifeq ($(COSMOE_DEBIAN_RULES),1)
deb:
	rm -rf $(DEB_BUILDDIR) $(DEB_DESTDIR)
	meson setup $(DEB_BUILDDIR) --prefix=/usr --libdir=lib/$(DEB_MULTIARCH) \
		-Denable_unit_test_compile=false
	ninja -C $(DEB_BUILDDIR)
	DESTDIR=$(DEB_DESTDIR) ninja -C $(DEB_BUILDDIR) install
else
deb:
	rm -rf debian/packages
	dpkg-buildpackage -us -uc -b
	mkdir -p debian/packages
	mv ../cosmoe*.deb debian/packages/
endif

# Windows cross-compilation using MXE on Linux or WSL
windows:
	@echo "Building native rc-bootstrap for cross-compilation..."
	meson setup builddir --reconfigure
	ninja -C builddir src/bin/rc/rc-bootstrap
	@echo "Configuring for Windows cross-compilation using MXE..."
	meson setup --cross-file cross-mxe.ini $(BUILDDIR)-windows --reconfigure \
		-Dnative_rc_path=$(CURDIR)/builddir/src/bin/rc/rc-bootstrap
	@echo "Building Windows binaries..."
	ninja -C $(BUILDDIR)-windows
	@echo ""
	@echo "Windows build complete! Build artifacts are in $(BUILDDIR)-windows/"
	./collect-windows-binaries.sh
	@echo "Windows apps and libraries collected into win-test/ directory"

windows-clean:
	rm -rf $(BUILDDIR)-windows
	rm -rf win-test
	@echo "Windows build directory and win-test directory removed"

.PHONY: build configure install clean distclean deb windows windows-clean