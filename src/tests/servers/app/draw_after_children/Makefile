# Makefile for compiling on Haiku only

CFLAGS   = -g -c -Wall -Wno-multichar -I. \
	-I/boot/system/develop/headers/private/interface \
	-I/boot/system/develop/headers/private/shared
CPPFLAGS = $(CFLAGS) -Woverloaded-virtual -Wnon-virtual-dtor
CXX = g++

EXE	= draw_after_children
OBJS := DrawAfterChildren.o


$(EXE): $(OBJS) Makefile
	$(CXX) $(OBJS) -lbe -o $@

$(OBJS): MacOSCompatibility.h

clean:
	-rm -f $(EXE) $(OBJS) *~

distclean: clean
	-rm -f Makefile

MacOSCompatibility.h:
	touch MacOSCompatibility.h

.PHONY: clean distclean deps doc install uninstall all