Welcome to Cosmoe!
https://www.cosmoe.org

WHAT IS COSMOE ON WAYLAND (COW)
-------------------------------
Cosmoe comes in 2 flavors: this light-weight Wayland-based implementation, and a
more complete reimplementation of Haiku.  This is the light-weight Wayland version,
which is likely to be the better supported version going forward.

Both are forks of the Haiku operating system, which itself is an open-source
re-implementation of BeOS.  Cosmoe differs from Haiku in that it uses the Linux
kernel instead of the custom Haiku kernel, and can run on any filesystem (not just
BeFS).

This project has just released, and is alpha-level software!  There are many bugs,
but I wanted to get a proof-of-concept out there.


PREREQUISITES
-------------
Your Linux installation must have the following installed:
 - a recent version of gcc or clang
 - libwayland and associated development headers/libraries
 - libpng and associated development headers/libraries
 - libjpg and associated development headers/libraries
 - libicu and associated development headers/libraries
 - bison and flex

Cosmoe on Wayland has been compiled and successfully tested under the following operating systems:
 - Ubuntu 24.04 x86-64

On Ubuntu/Debian systems, all prerequisites can be installed with:

```sudo apt install gcc g++ flex bison libpng-dev libjpeg-dev libicu-dev libfreetype6-dev libcppunit-dev```


Under Fedora/Redhat, all prerequisites can be installed with:

```sudo dnf install gcc g++ flex bison libpng-devel libjpeg-devel libicu-devel freetype-devel cppunit-devel```


INSTALLATION
------------
Cosmoe is built with meson and ninja:

setup/configure:

```meson setup builddir```

build:

```ninja -C builddir```

install:

```ninja -C builddir install```

Note that I've chosen "builddir" as the build directory name, but it can be named whatever you want (except "build" ironically, as
we have existing build-related files from Haiku in there).



RUNNING COSMOE APPS
-------------------
To launch a Cosmoe-based app, simply run it while using any Wayland-based graphical
environment.  Several sample Cosmoe apps are included with this distribution, including:
guido
Mandelbrot
Gradients
Pairs
AboutSystem
Sudoku
Terminal
Clock
DeskCalc


Unlike the "classic" version of Cosmoe, there is no "cosmoe.sh" to run, and apps launch
right in the graphical environment you are already using.




PROBLEMS
--------
If the Cosmoe hangs and you are unable to kill the Cosmoe SDL window:
1. Type ctrl-z in the shell that launched Cosmoe
2. Type xkill and select the Cosmoe window to remove it
3. Type "kill %1" in the shell that launched Cosmoe

If Cosmoe fails to compile for you, please file an issue at gitlab.

If a Cosmoe app crashes, file an issue at gitlab and send me a backtrace.

If the appserver crashes you can check the file server.out for some
hopefully helpful information.

Please see the TODO file for a list of issues and possible workarounds.


API DOCUMENTATION
-----------------
Since Cosmoe strives to conform to the Be API, the best API
documentation to use is the BeBook, available at several sites
online.

https://www.haiku-os.org/legacy-docs/bebook

