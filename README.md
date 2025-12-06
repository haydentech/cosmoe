Welcome to Cosmoe!
https://www.cosmoe.org

WHAT IS COSMOE
-------------------------
Cosmoe is a library that allows developers to build rich, easy-to-code, native
Linux apps with the BeOS API.

Cosmoe comes in 2 flavors: this light-weight UI library, and a more complete
reimplementation of the Haiku OS called Cosmoe Classic.  This light-weight
version is newer and likely to be the better supported version going
forward.  Cosmoe allows you run apps using the BeOS API directly on Linux,
running under either a Wayland-based or X11-based graphical enviroment.

Both versions of Cosmoe descend from the Haiku operating system, which itself is an
open-source re-implementation of BeOS.  Cosmoe differs from Haiku in that it uses the
Linux kernel instead of the Haiku kernel, and can run on any filesystem (not just
BeFS).

This project has just publicly released, and is alpha-level software!  There are many bugs,
but I wanted to get a proof-of-concept out there.


PREREQUISITES
-------------
Your Linux installation must have the following installed:
 - gcc or clang compilers
 - libwayland, libpng, libjpg, libwebp, libicu, libfreetype, libpango, libpixman, libxkbcommon,
 	libxkbcommon-x11 and associated development headers/libraries
 - bison and flex
 - meson and ninja
 - an X11 or Wayland-based graphical enviroment (Weston and dwl have been successfully used)

Cosmoe has been compiled and successfully tested under the following operating systems:
 - Ubuntu 24.04
 - Arch Linux
 - Fedora Core 40 and 43

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


INSTALLATION
------------
Cosmoe is built with meson and ninja:

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

If you have decades of muscle-memory of typing `make` and `make install`, like me, no
problem!  Those make commands will run the correct meson/ninja jobs.


RUNNING COSMOE APPS
-------------------
To launch a Cosmoe-based app, simply run it while using any Wayland or X11 graphical
environment.  Several sample Cosmoe apps are installed by this distribution, including:
- guido
- Mandelbrot
- Clock
- FontDemo
- Pulse
- Gradients
- DeskCalc
- Pairs
- AboutSystem
- Terminal
- Sudoku
- DriveUsage
- StyledEdit

Note that not all of them work well at the moment, and some barely at all.  I've listed
them roughly in the order of their stability and conformance to their behavior on Haiku.
Guido is my testbed for implementing new BeOS API functionality, so it's by far the best
example of what Cosmoe can accomplish as a UI library.

Unlike the "Classic" version of Cosmoe, there is no "cosmoe.sh" to run, and apps launch
right in the graphical environment you are already using.


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

