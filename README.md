Welcome to Cosmoe!
https://www.cosmoe.org

WHAT IS COSMOE
--------------
Cosmoe is a library that allows developers to build rich, easy-to-code apps using
the BeOS API which compile and run on Linux (X11 & Wayland), MacOS, and Windows.

Cosmoe descends from the Haiku operating system, which itself is an open-source
re-implementation of BeOS.  Whereas Haiku is a full, standalone operating system, Cosmoe
is a class library that run on all major operating system and windowing engines.

This project has only recently publicly released, and is alpha-level software!
Come join in the project and make it great!


PREREQUISITES
-------------
Your system must have the following installed:

**Required:**
 - gcc or clang compilers
 - meson and ninja
 - bison and flex
 - libpng, libicu, libfreetype, libpango, libfontconfig, libglib development headers/libraries
 - For Linux: libwayland, libpixman, libxkbcommon, libxkbcommon-x11 and associated headers

**Optional, but recommended (for image format support):**
 - libjpeg (Linux) or jpeg-turbo (Mac)
 - libwebp
 - If these are not installed, the jpeg and webp image translators won't be available

Cosmoe has been compiled and successfully tested under the following operating systems:
 - Ubuntu 24.04
 - Arch Linux
 - Fedora Core 40 and 43
 - macOS 14
 - WINE 9.0
 - Windows 11

### Linux Prerequisites

On Ubuntu/Debian systems, all prerequisites can be installed with:

```
sudo apt install gcc g++ flex bison libpng-dev libjpeg-dev libwebp-dev libicu-dev libfreetype6-dev libpango1.0-dev libpixman-1-dev libxkbcommon-dev libwayland-dev libcppunit-dev pkg-config meson libxkbcommon-x11-dev
```

Under Fedora/Redhat, all prerequisites can be installed with:

```
sudo dnf install gcc g++ flex bison libpng-devel libjpeg-devel libwebp-devel libicu-devel freetype-devel pango-devel libxkbcommon-devel wayland-devel cppunit-devel meson libxkbcommon-x11-devel
```

Under Arch Linux, all prerequisites can be installed with:
```
sudo pacman -S python meson pkg-config libwebp gcc binutils make flex bison
```

### macOS Prerequisites

On macOS, install prerequisites using [Homebrew](https://brew.sh/):

```bash
# Install Xcode Command Line Tools (if not already installed)
xcode-select --install

# Install Homebrew (if not already installed)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install build dependencies
brew install meson ninja pkg-config cairo pango libpng jpeg-turbo webp icu4c freetype fontconfig glib
```

### Windows Prerequisites

The Windows version is cross-compiled on Linux using MinGW64 MXE, and tested with WINE.


LINUX BUILD
-----------
Cosmoe is built with meson and ninja, but you can use ```make``` and ```make install``` too if that
is more familiar to you.

setup/configure:

```meson setup builddir```

build:

```ninja -C builddir```

install:

```ninja -C builddir install```

Graphics Backends:
You can disable building the Wayland or X11 backend at configuration time.
To disable a backend, pass the option to meson when creating your build directory.  Alternatively, you can edit meson_options.txt.

```bash
meson setup builddir -Denable_wayland_backend=false -Denable_x11_backend=true
ninja -C builddir
```

Note that I've chosen "builddir" as the build directory name, but it can be named
whatever you want (except "build" ironically, as Haiku stores its build-related files
in there, and we match their directory structure).


MAC BUILD
---------

The Mac build is performed on a dynamically created case-sensitive disk image.  This image is created
by our Makefile, so all you need is a standard...

```make```
```make install```

Command-line programs are installed to /usr/local/bin and graphical programs are installed to
/usr/local/Applications.  Fonts are installed to `~/Library/Fonts/Cosmoe`.


WINDOWS BUILD
-------------

The Windows version is cross-compiled in WSL or Linux using [MXE (M cross environment)](https://mxe.cc/).

### MXE Installation

1. Install prerequisites and link python (Ubuntu installation shown)

```bash
sudo apt install automake autoconf libtool ruby unzip lzip gperf autopoint 7zip intltool libtool-bin python3-mako libssl-dev libpcre2-dev
sudo ln -s /usr/bin/python3 /usr/local/bin/python
```

2. Clone MXE:

```bash
git clone https://github.com/mxe/mxe.git ~/mxe
```

3. Build required MXE packages:

```bash
cd ~/mxe
make MXE_TARGETS=x86_64-w64-mingw32.shared \
     gcc cairo pango fontconfig freetype icu4c libpng jpeg libwebp
```

4. Ensure Cosmoe's cross-compilation file `cross-mxe.ini` accurately represents your MXE paths

### Building for Windows

```bash
meson setup build-windows --cross-file cross-mxe.ini
ninja -C build-windows
```

Alternatively, you may also use make:

```bash
make windows-mxe
```

### Testing with WINE

Copy the dll's and exe's from the build to a test directory:

```bash
mkdir win-test
find build-windows -name "*.exe" -exec cp {} win-test/ \;
find build-windows -name "*.dll" -exec cp {} win-test/ \;
cp ~/mxe/usr/x86_64-w64-mingw32.shared/bin/{libwinpthread-1.dll,libcairo-2.dll,libglib-2.0-0.dll,libgobject-2.0-0.dll,libiconv-2.dll,icuin74.dll,icuuc74.dll,libpango-1.0-0.dll,libpangocairo-1.0-0.dll,libgcc_s_seh-1.dll,libstdc++-6.dll,libffi-8.dll,libfontconfig-1.dll,libfreetype-6.dll,libpixman-1-0.dll,libpng16-16.dll,zlib1.dll,libintl-8.dll,libpcre2-8-0.dll,libfribidi-0.dll,libgio-2.0-0.dll,libharfbuzz-0.dll,libpangoft2-1.0-0.dll,libpangowin32-1.0-0.dll,icudt74.dll,libexpat-1.dll,libbrotlidec.dll,libbz2.dll,libgmodule-2.0-0.dll,libbrotlicommon.dll} win-test/
```

On Linux, run any app through WINE (e.g. guido):
```bash
cd win-test
wine guido.exe
```

On Windows, simply double-click the app as usual.


RUNNING COSMOE APPS
-------------------
On Linux, simply run the app from the command line while using either Wayland or X11.
On macOS, run apps from the command line, or go to `/usr/local/Applications`
in the Finder and double-click to launch as usual.
For Windows builds, run apps with WINE or move the files to a Windows system.

Several sample Cosmoe apps are installed by this distribution, including:
- guido
- Mandelbrot *
- Clock
- FontDemo
- Pulse
- Gradients
- DeskCalc
- Pairs *
- AboutSystem *
- Terminal *
- Sudoku *
- DriveUsage *
- StyledEdit *

Note that not all of them work well at the moment.  I've listed
them roughly in the order of their stability and conformance to their behavior on Haiku.
Guido is my testbed for implementing new BeOS API functionality, so it's by far the best
example of what Cosmoe can accomplish as a UI library.  Starred apps (*) are currently not
working yet on macOS.


PROBLEMS
--------
Cosmoe is very much a work in progress.  If Cosmoe fails to compile for you, or
an app crashes or displays incorrect behavior, please file an issue at gitlab.

Please see the TODO file for a list of issues and possible workarounds.


API DOCUMENTATION
-----------------
Since Cosmoe strives to conform to the Be API, the best API documentation to use is the
BeBook, available at several sites online.

https://www.haiku-os.org/legacy-docs/bebook

