# Makefile for compiling on Haiku only

CFLAGS   = -g -c -Wall -Wno-multichar -I. \
	-I/boot/system/develop/headers/private/interface \
	-I/boot/system/develop/headers/private/tracker \
	-I/boot/system/develop/headers/private/shared
CPPFLAGS = $(CFLAGS) -Woverloaded-virtual -Wnon-virtual-dtor
CXX = g++

EXE	= gui-torture
OBJS := app.o disapp.o diswindow.o disview.o


$(EXE): $(OBJDIR) $(OBJS) Makefile
	$(CXX) $(OBJS)  -lbe -ltranslation -ltracker -o $@

$(OBJS): MacOSCompatibility.h

$(OBJDIR):
	mkdir $(OBJDIR)

install: $(EXE) $(bindir)
	cp -f $(EXE) $(bindir)

$(bindir):
	mkdir -p $(bindir)

clean:
	-rm -f $(EXE) $(OBJS) *~

distclean: clean
	-rm -f Makefile
	-rm -rf $(OBJDIR)

MacOSCompatibility.h:
	touch MacOSCompatibility.h

.PHONY: clean distclean deps doc install uninstall all
