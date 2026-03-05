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

```bash
sudo apt install gcc g++ flex bison libpng-dev libjpeg-dev libwebp-dev libicu-dev libfreetype6-dev libpango1.0-dev libpixman-1-dev libxkbcommon-dev libwayland-dev libcppunit-dev pkg-config meson libxkbcommon-x11-dev
```

Under Fedora/Redhat, all prerequisites can be installed with:

```bash
sudo dnf install gcc g++ flex bison libpng-devel libjpeg-devel libwebp-devel libicu-devel freetype-devel pango-devel libxkbcommon-devel wayland-devel cppunit-devel meson libxkbcommon-x11-devel
```

Under Arch Linux, all prerequisites can be installed with:
```bash
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

The Windows version is cross-compiled on Linux or WSL using MinGW64 MXE, and tested with WINE.

1. In WSL, install prerequisites and link python

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


LINUX BUILD
-----------
Cosmoe is built and installed with a standard...
```bash
make
make install
``` 

The build objects are placed in "builddir" if you need them.

Programs are installed to `/usr/local/bin`.


MAC BUILD
---------

The Mac build and install is also performed with ```make``` and ```make install```.

Since the build requires a case-senstive volume, ```make``` creates a suitable disk image and
does the compilation there.  You may notice this volume mounted in the Finder.

Command-line programs are installed to `/usr/local/bin` and graphical programs are installed to
`/usr/local/Applications`.  Fonts are installed to `~/Library/Fonts/Cosmoe`.


WINDOWS BUILD
-------------

The Windows build is a bit more involved.  It is cross-compiled in WSL or Linux using [MXE (M cross environment)](https://mxe.cc/).

You need an installed Linux build on the build machine first, since that provides some needed build tools.
After that you can build the Windows version:

```bash
make
make install
```

Edit Cosmoe's cross-compilation file `cross-mxe.ini` to ensure it accurately represents your MXE paths

```bash
make windows
```

On Windows, apps are not installed yet, and the exe's and dll's must be collected
into a test folder after building:
```bash
./collect-windows-binaries.sh
```

RUNNING COSMOE APPS
-------------------
Apps may be started from the commandline or double-clicked in your desktop environment.

If running Windows apps under Wine, launch like so:
```bash
env PANGOCAIRO_BACKEND=fontconfig wine ./Showcase.exe
```

Several sample Cosmoe apps are built by this distribution, including:
- Showcase
- Mandelbrot
- Clock
- Pulse
- FontDemo
- Gradients
- ShowImage
- CharacterMap
- DeskCalc
- Pairs
- AboutSystem
- Terminal
- StyledEdit
- DriveUsage
- Icon-O-Matic
- Sudoku

Note that not all of them work well at the moment.  I've listed them roughly
in the order of their stability and conformance to their behavior on Haiku.
Showcase (formerly Guido) is my testbed for implementing new BeOS API
functionality, so it's by far the best example of what Cosmoe can accomplish
as a UI library.


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

