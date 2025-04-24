dnl Process this file with autoconf to produce a configure script.
AC_INIT
AC_CONFIG_SRCDIR([src/kits/interface/View.cpp])
AC_CONFIG_HEADERS([config.h])

m4_include(ax_gcc_builtin.m4)

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

AC_PROG_LEX(noyywrap)
if test "x$LEX" != xflex; then
	AC_MSG_ERROR([*** Cosmoe requires flex.])
fi

AC_DEFINE([_GNU_SOURCE], [1],
  [Define _GNU_SOURCE so that we get all necessary prototypes]
)

AC_DEFINE([_ALL_SOURCE], [1],
  [Define _ALL_SOURCE so that we get everything]
)

AC_DEFINE([USE_RESIZE_POOL], [1],
  [Define USE_RESIZE_POOL to optimize window resize performance]
)

AC_DEFINE([DATADIR], ["/usr/local/share"],
  [Define DATADIR]
)


dnl Determine our platform.
AC_CANONICAL_HOST

dnl Checks for libraries.
AC_CHECK_LIB(stdc, printf)

dnl Checks for header files.
AC_HEADER_DIRENT

LIBEXT="so"
SHAREDLINK="-shared"
STRCASESTROBJ=
FONTDIR="/usr/share/fonts/ttf/cosmoe"

ABS_TOPDIR=`pwd`
RC="LD_LIBRARY_PATH=$(pwd)/src/kits/objs $(pwd)/src/bin/rc/rc"
XRES="LD_LIBRARY_PATH=$(pwd)/src/kits/objs $(pwd)/src/bin/xres"
MIMESET="LD_LIBRARY_PATH=$(pwd)/src/kits/objs $(pwd)/src/bin/mimeset"

AC_CHECK_LIB(pthread, pthread_kill, ,
  AC_MSG_ERROR([
*** Cosmoe requires a libpthread with pthread_kill support.]))

AC_CHECK_LIB([wayland-client], [wl_display_connect], ,
  AC_MSG_ERROR([
*** Cosmoe requires libwayland-client (package libwayland-client0 and libwayland-dev).])
)

AC_CHECK_LIB([wayland-cursor], [wl_cursor_theme_load], ,
  AC_MSG_ERROR([
*** Cosmoe requires libwayland-cursor (packages libwayland-cursor0 and libwayland-dev).])
)

AC_CHECK_LIB([xkbcommon], [xkb_context_new], ,
  AC_MSG_ERROR([
*** Cosmoe requires libxkbcommon (packages libxkbcommon0 and libxkbcommon-dev).])
)

AC_CHECK_LIB([cairo], [cairo_create], ,
  AC_MSG_ERROR([
*** Cosmoe requires libcairo (packages libcairo2 and libcairo2-dev).])
)

AC_CHECK_LIB([pango-1.0], [pango_font_description_from_string], ,
  AC_MSG_ERROR([
*** Cosmoe requires libpango (packages libpango-1.0 and libpango1.0-dev).])
)

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
  AC_DEFINE(HAVE_SYS_XATTR_H, [], [OS has generic filesystem attribute support]),
  AC_MSG_WARN([
*** This build of Cosmoe will not support attributes.]))

AC_CHECK_FUNC([posix_fallocate], [
  AC_DEFINE([HAVE_POSIX_FALLOCATE], [1], [Define if posix_fallocate is available])
], [
])

AC_CHECK_FUNC([memfd_create], [
  AC_DEFINE([HAVE_MEMFD_CREATE], [1], [Define if memfd_create is available])
], [
])

AC_CHECK_FUNC([strchrnul], [
  AC_DEFINE([HAVE_STRCHRNUL], [1], [Define if strchrnul is available])
], [  
])

AC_CHECK_FUNC([mkostemp], [
  AC_DEFINE([HAVE_MKOSTEMP], [1], [Define if mkostemp is available])
], [
])

AC_CHECK_FUNC([xkb_compose_table_new_from_file], [
  AC_DEFINE([HAVE_XKBCOMMON_COMPOSE], [1], [Define if xkb_compose_table_new_from_file is available])
], [
])

AX_GCC_BUILTIN([__builtin_bswap32])

if test "$ax_cv_have___builtin_bswap32" = "yes"; then
  AC_DEFINE([HAVE_BUILTIN_BSWAP32], [1], [Define if __builtin_bswap32 is available])
fi

AX_GCC_BUILTIN([__builtin_clz])

if test "$ax_cv_have___builtin_clz" = "yes"; then
  AC_DEFINE([HAVE_BUILTIN_CLZ], [1], [Define if __builtin_clz is available])
fi

AX_GCC_BUILTIN([__builtin_popcount])

if test "$ax_cv_have___builtin_popcount" = "yes"; then
  AC_DEFINE([HAVE_BUILTIN_POPCOUNT], [1], [Define if __builtin_popcount is available])
fi

AX_GCC_BUILTIN([__builtin_unreachable])

if test "$ax_cv_have___builtin_unreachable" = "yes"; then
  AC_DEFINE([HAVE_UNREACHABLE], [1], [Define if __builtin_unreachable is available])
fi

AC_CHECK_HEADER(webp/decode.h,
  AC_DEFINE(HAVE_WEBP, [1], [WebP headers are available]),)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE

dnl Checks for library functions.
AC_PROG_GCC_TRADITIONAL


AC_CHECK_FUNC([strlcat], [AC_DEFINE([HAVE_STRLCAT], [1], [Define if strlcat exists.])])

AC_CHECK_FUNC([strlcpy], [AC_DEFINE([HAVE_STRLCPY], [1], [Define if strlcpy exists.])],
	STRLCPY_DEFINE=-DSTRLCPY_MISSING
)

dnl At this point, we know pango exists
AC_DEFINE([HAVE_PANGO], [1], [Define if pango is available])

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

AC_SUBST(HAVE_POSIX_FALLOCATE)
AC_SUBST(HAVE_MEMFD_CREATE)
AC_SUBST(HAVE_STRCHRNUL)
AC_SUBST(HAVE_MKOSTEMP)
AC_SUBST(HAVE_XKBCOMMON_COMPOSE)
AC_SUBST(HAVE_BUILTIN_BSWAP32)
AC_SUBST(HAVE_BUILTIN_CLZ)
AC_SUBST(HAVE_BUILTIN_POPCOUNT)
AC_SUBST(HAVE_UNREACHABLE)
AC_SUBST(HAVE_WEBP)

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
src/apps/mandelbrot/Makefile
src/apps/minimal/Makefile
src/apps/sudoku/Makefile
src/apps/terminal/Makefile

src/add-ons/Makefile
src/add-ons/translators/Makefile
src/add-ons/translators/bmp/Makefile
src/add-ons/translators/png/Makefile
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
dnl src/apps/pulse/Makefile
dnl src/apps/resedit/Makefile
dnl src/apps/showimage/Makefile
dnl src/apps/stylededit/Makefile
dnl src/apps/testharness/Makefile
dnl src/apps/tracker/Makefile
dnl src/apps/osutils/Makefile
dnl src/apps/deskbar/Makefile

dnl src/servers/registrar/Makefile
dnl src/servers/app/Makefile
dnl src/servers/app/drawing/Painter/Makefile
dnl src/tests/system/kernel/Makefile
dnl src/kits/tracker/Makefile

dnl src/libs/icon/Makefile
