dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_SRCDIR([src/kits/interface/View.cpp])
AC_CONFIG_HEADERS([config.h])

dnl Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AC_PROG_RANLIB

dnl Determine our platform.
AC_CANONICAL_HOST

dnl Checks for libraries.
AC_CHECK_LIB(stdc, printf)

dnl Checks for header files.
AC_HEADER_DIRENT

case "$host" in
	*darwin*)	LIBEXT="dylib"
				SHAREDLINK="-dynamiclib"
				STRCASESTROBJ="string_helper.o"
				FONTDIR="/Library/Fonts/cosmoe"
				;;
	*)			LIBEXT="so"
				SHAREDLINK="-shared"
				STRCASESTROBJ=
				FONTDIR="/usr/share/fonts/ttf/cosmoe"
				;;
esac

ABS_TOPDIR=`pwd`
RC="LD_LIBRARY_PATH=$(pwd)/src/kits/objs $(pwd)/src/bin/rc/rc"
XRES="LD_LIBRARY_PATH=$(pwd)/src/kits/objs $(pwd)/src/bin/xres"
MIMESET="LD_LIBRARY_PATH=$(pwd)/src/kits/objs $(pwd)/src/bin/mimeset"

AC_CHECK_LIB(pthread, pthread_kill, ,
  AC_MSG_ERROR([
*** Cosmoe requires a libpthread with pthread_kill support.]))

AC_CHECK_LIB(dl, dlopen, ,
  AC_MSG_ERROR([
*** Cosmoe requires libdl.]))

AC_CHECK_HEADER(png.h,
  AC_DEFINE(COSMOE_PNG, [], [PNG libraries/headers are available]),
  AC_MSG_WARN([
*** libjpeg header files not found.  PNG translator will not be built.]))

AC_CHECK_LIB(png, png_read_info, ,
  AC_MSG_ERROR([
*** Cosmoe requires libpng.]),
  -lz -lm)

AC_CHECK_HEADER(zlib.h, ,
  AC_MSG_ERROR([
*** Cosmoe requires the libz header files to support libpng.]))

AC_CHECK_LIB(jpeg, jpeg_destroy_decompress, ,
  AC_MSG_ERROR([
*** Cosmoe requires libjpeg.]),
  -lz -lm)

AC_CHECK_HEADER(jpeglib.h,
  AC_DEFINE(COSMOE_JPEG, [], [JPEG libraries/headers are available]),
  AC_MSG_WARN([
*** libjpeg header files not found.  JPEG translator will not be built.]))

AC_CHECK_HEADER(sys/xattr.h,
  AC_DEFINE(COSMOE_ATTRIBUTES, [], [OS has generic filesystem attribute support]),
  AC_MSG_WARN([
*** This build of Cosmoe will not support attributes.]))

AC_CHECK_PROG(FREETYPE_CONFIG, pkg-config freetype2, yes)
if eval "test x$FREETYPE_CONFIG != xyes"; then
   AC_MSG_ERROR([
*** Cosmoe requires libfreetype version 2.0 or later.])
fi

AC_CHECK_PROG(ICU_CONFIG, pkg-config icu-uc, yes)
if eval "test x$ICU_CONFIG != xyes"; then
   AC_MSG_ERROR([
*** Cosmoe requires the ICU libraries.])
fi

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE

dnl Checks for library functions.
AC_PROG_GCC_TRADITIONAL

VIDEODRVLIB=""
VIDEODRVOBJ="VesaInterface.o"

AC_MSG_CHECKING(whether to enable SDL graphics rendering)
AC_ARG_ENABLE(sdl, [  --enable-sdl            run Cosmoe on top of SDL [default=no]],
 if eval "test x$enable_sdl = xyes"; then
   VIDEODRVLIB=`pkg-config sdl2 --libs`
   VIDEODRVOBJ="SDLInterface.o"
   VIDEODRVCFLAGS=`pkg-config sdl2 --cflags`
   VESAOBJ=""
   INPUTDRV=
   AC_DEFINE(COSMOE_SDL)

   AC_MSG_RESULT(yes)
 else
   AC_MSG_RESULT(no)
 fi
 , AC_MSG_RESULT(no))

AC_SUBST(VIDEODRVOBJ)
AC_SUBST(VIDEODRVLIB)
AC_SUBST(VIDEODRVCFLAGS)
AC_SUBST(LIBEXT)
AC_SUBST(SHAREDLINK)
AC_SUBST(INPUTDRV)
AC_SUBST(STRCASESTROBJ)
AC_SUBST(HAS_ATTR_SUPPORT)
AC_SUBST(FONTDIR)

AC_SUBST(ABS_TOPDIR)

AC_SUBST(XRES)
AC_SUBST(RC)
AC_SUBST(MIMESET)

AC_CONFIG_FILES([
Makefile
cosmoe.specs
server.specs
icon.specs
makefile.rules
headers/Makefile
src/bin/Makefile
src/bin/rc/Makefile
src/kits/Makefile
src/kits/tracker/Makefile
src/libs/agg/Makefile
src/libs/icon/Makefile
src/libs/linprog/Makefile
src/libs/mapm/Makefile
src/apps/Makefile
src/apps/bedate/Makefile
src/apps/deskbar/Makefile
src/apps/guido/Makefile
src/apps/calc/Makefile
src/apps/clock/Makefile
src/apps/cterm/Makefile
src/apps/deskcalc/Makefile
src/apps/fontdemo/Makefile
src/apps/gradients/Makefile
src/apps/magnify/Makefile
src/apps/mandelbrot/Makefile
src/apps/pulse/Makefile
src/apps/resedit/Makefile
src/apps/stylededit/Makefile
src/apps/sudoku/Makefile
src/apps/terminal/Makefile
src/apps/testharness/Makefile
src/apps/tracker/Makefile
src/apps/osutils/Makefile
src/servers/registrar/Makefile
src/servers/app/Makefile
src/servers/app/drawing/Painter/Makefile
src/tests/system/kernel/Makefile
src/add-ons/Makefile
src/add-ons/translators/Makefile
src/add-ons/translators/bmp/Makefile
src/add-ons/translators/png/Makefile
])
AC_OUTPUT

dnl add-ons/translators/jpg2000/Makefile
dnl add-ons/translators/jpg/Makefile
dnl add-ons/translators/libtiff/Makefile
dnl add-ons/translators/png/Makefile
dnl add-ons/translators/ppm/Makefile
dnl add-ons/translators/sgi/Makefile
dnl add-ons/translators/stxt/Makefile
dnl add-ons/translators/tga/Makefile
dnl add-ons/translators/tiff/Makefile
