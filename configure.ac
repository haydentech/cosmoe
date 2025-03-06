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

AC_PROG_YACC
if test x"$YACC" = "xyacc"; then
  AC_CHECK_PROG([YACC_EXISTS], [yacc], [yes], [no])
  if test x"$YACC_EXISTS" != xyes; then
    AC_MSG_ERROR([*** Cosmoe requires bison.])
  fi
fi

AC_PROG_LEX
if test "x$LEX" != xflex; then
	AC_MSG_ERROR([*** Cosmoe requires flex.])
fi


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
*** libpng header files not found.  PNG translator will not be built.]))

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

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE

dnl Checks for library functions.
AC_PROG_GCC_TRADITIONAL


AC_CHECK_FUNC([strlcat], [AC_DEFINE([HAVE_STRLCAT], [1], [Define if strlcat exists.])])

AC_CHECK_FUNC([strlcpy], [AC_DEFINE([HAVE_STRLCPY], [1], [Define if strlcpy exists.])],
	STRLCPY_DEFINE=-DSTRLCPY_MISSING
)

AC_SUBST(LIBEXT)
AC_SUBST(SHAREDLINK)
AC_SUBST(INPUTDRV)
AC_SUBST(STRCASESTROBJ)
AC_SUBST(HAS_ATTR_SUPPORT)
AC_SUBST(FONTDIR)
AC_SUBST(STRLCPY_DEFINE)
AC_SUBST(ABS_TOPDIR)

AC_SUBST(XRES)
AC_SUBST(RC)
AC_SUBST(MIMESET)

AC_CONFIG_FILES([
Makefile
cosmoe.specs
icon.specs

makefile.rules
headers/Makefile

src/libs/linprog/Makefile
src/libs/mapm/Makefile
src/libs/agg/Makefile
src/libs/icon/Makefile

src/bin/Makefile
src/bin/rc/Makefile

src/kits/Makefile

src/apps/Makefile

src/apps/guido/Makefile
src/apps/aboutsystem/Makefile
src/apps/fontdemo/Makefile
src/apps/deskcalc/Makefile
src/apps/gradients/Makefile
src/apps/bedate/Makefile
src/apps/clock/Makefile
src/apps/mandelbrot/Makefile
src/apps/sudoku/Makefile

])
AC_OUTPUT

dnl server.specs 
dnl add-ons/translators/jpg2000/Makefile
dnl add-ons/translators/jpg/Makefile
dnl add-ons/translators/libtiff/Makefile
dnl add-ons/translators/png/Makefile
dnl add-ons/translators/ppm/Makefile
dnl add-ons/translators/sgi/Makefile
dnl add-ons/translators/stxt/Makefile
dnl add-ons/translators/tga/Makefile
dnl add-ons/translators/tiff/Makefile
dnl src/apps/calc/Makefile
dnl src/apps/cterm/Makefile
dnl src/apps/magnify/Makefile
dnl src/apps/pulse/Makefile
dnl src/apps/resedit/Makefile
dnl src/apps/screenshot/Makefile
dnl src/apps/showimage/Makefile
dnl src/apps/stylededit/Makefile
dnl src/apps/terminal/Makefile
dnl src/apps/testharness/Makefile
dnl src/apps/tracker/Makefile
dnl src/apps/osutils/Makefile
dnl src/apps/deskbar/Makefile
dnl src/add-ons/translators/Makefile
dnl src/add-ons/translators/bmp/Makefile
dnl src/add-ons/translators/png/Makefile
dnl src/servers/registrar/Makefile
dnl src/servers/app/Makefile
dnl src/servers/app/drawing/Painter/Makefile
dnl src/tests/system/kernel/Makefile
dnl src/add-ons/Makefile
dnl src/kits/tracker/Makefile
dnl 
dnl src/libs/icon/Makefile
