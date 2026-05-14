# Makefile for compiling on Haiku only

CFLAGS   = -g -c -Wall -Wno-multichar -I. \
	-I/boot/system/develop/headers/private/interface \
	-I/boot/system/develop/headers/private/shared \
	-I../bitmap_drawing
CPPFLAGS = $(CFLAGS) -Woverloaded-virtual -Wnon-virtual-dtor
CXX = g++

EXES := cursor_test cursor_bitmap_test
CURSOR_TEST_OBJS := CursorTest.o
CURSOR_BITMAP_TEST_OBJS := CursorBitmapTest.o


all: $(EXES)

cursor_test: $(CURSOR_TEST_OBJS) Makefile
	$(CXX) $(CURSOR_TEST_OBJS) -lbe -o $@

cursor_bitmap_test: $(CURSOR_BITMAP_TEST_OBJS) Makefile
	$(CXX) $(CURSOR_BITMAP_TEST_OBJS) -lbe -o $@

$(CURSOR_TEST_OBJS) $(CURSOR_BITMAP_TEST_OBJS): MacOSCompatibility.h

clean:
	-rm -f $(EXES) $(CURSOR_TEST_OBJS) $(CURSOR_BITMAP_TEST_OBJS) *~

distclean: clean
	-rm -f Makefile

MacOSCompatibility.h:
	touch MacOSCompatibility.h

.PHONY: all clean distclean deps doc install uninstall