prefix=/usr/local
bindir=$(prefix)/bin

CFLAGS   = -g -c -Wall -Wno-multichar `cat ../../../cosmoe.specs`
CXXFLAGS = $(CFLAGS) -Woverloaded-virtual -Wnon-virtual-dtor

CC	= gcc -m32
CXX	= g++ -m32
LD	= g++ -m32
EXE	= cterm
COSMOELIBDIR = ../../../src/kits/objs

OBJDIR	:= objs


$(OBJDIR)/%.o : %.c
	$(CC) $(CFLAGS) $< -o $@

$(OBJDIR)/%.o : %.cpp
	$(CXX) $(CXXFLAGS) $< -o $@


OBJS := main.o tview.o
OBJS := $(addprefix $(OBJDIR)/,$(OBJS))



$(EXE): $(OBJS) Makefile
	$(CXX) $(OBJS) -L$(COSMOELIBDIR) -lcosmoe -o $@

objs:
	mkdir objs

install: $(EXE) $(bindir)
	cp -f $(EXE) $(bindir)
	strip --strip-all $(bindir)/$(EXE)

$(bindir):
	mkdir -p $(bindir)

clean:
	-rm -f $(EXE) $(OBJS) *~

distclean: clean
	-rm -f Makefile

.cpp.o:
	$(CXX) $(CFLAGS) $< -o $@

main.o:		main.cpp tview.h
tview.o:	tview.cpp tview.h

.PHONY: clean distclean deps doc install uninstall all
