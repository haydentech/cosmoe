Welcome to Cosmoe!
https://www.cosmoe.org

WHAT IS COSMOE
--------------
Cosmoe is a fork of the Haiku operating system, which itself is an open-source
re-implementation of BeOS.  Cosmoe differs from Haiku in that it uses the Linux
kernel instead of the custom Haiku kernel, and can run on any filesystem (not just
BeFS).


PREREQUISITES
-------------
Your Linux installation must have the following installed:
 - a recent version of gcc or clang
 - libSDL 2 and associated development headers/libraries
 - libpng and associated development headers/libraries
 - libjpg and associated development headers/libraries
 - libicu and associated development headers/libraries
 - bison and flex
 - autoconf (if you want to make source code changes)

Cosmoe has been compiled and successfully tested under the following operating systems:
 - Ubuntu 22.04 x86-64
 - Fedora Core 40 x86-64
 - Fedora Core 40 AARM-64 (Asahi Linux on Mac M1)

On Ubuntu/Debian systems, all prerequisites can be installed with:

sudo apt install gcc g++ flex bison autoconf libsdl2-dev libpng-dev libjpeg-dev libicu-dev libfreetype6-dev  libcppunit-devel


Under Fedora/Redhat, all prerequisites can be installed with:

sudo dnf install gcc g++ flex bison autoconf SDL2-devel libpng-devel libjpeg-devel libicu-devel freetype-devel  cppunit-dev


INSTALLATION
------------
Cosmoe is built with configure and make, like most open-source software.  The most
common configuration is wrapped by the build.sh command, so from the Cosmoe
source directory, to install you can simply run:

- ./build.sh
- sudo make install


RUNNING COSMOE
--------------
To launch Cosmoe, run this from the root of the Cosmoe source directory:
- sudo ./cosmoe.sh

To quit Cosmoe, simply close the Cosmoe window.  If you are running in
fullscreen mode, press escape.


PROBLEMS
--------
If the Cosmoe hangs and you are unable to kill the Cosmoe SDL window:
1. Type ctrl-z in the shell that launched Cosmoe
2. Type xkill and select the Cosmoe window to remove it
3. Type "kill %1" in the shell that launched Cosmoe

If Cosmoe fails to compile for you, please let me know by e-mail.

If a Cosmoe app crashes, please send me a backtrace.

If the appserver crashes you can check the file server.out for some
hopefully helpful information.


API DOCUMENTATION
-----------------
Since Cosmoe strives to conform to the Be API, the best API
documentation to use is the BeBook, available at several sites
online.

https://www.haiku-os.org/legacy-docs/bebook


MAKING SOURCE CHANGES
---------------------
Source code improvements are welcomed!  If you are making source code
changes to libbe or the app_server, you should run "make deps" to ensure
that dependencies will be created and used.  Once this command is run,
the dependencies will automatically be updated from that point forward
until such time as a "make clean" or "make distclean" is performed.
