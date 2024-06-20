prefix=/usr/local
bindir=${prefix}/bin

COPTS	= `cat ../../cosmoe.specs` -g -Wall -Wno-multichar -c -I../../headers/tools/cppunit

OBJS	= UnitTester.o
EXE		= unittester

OBJDIR	:= objs

COSMOELIBDIR = ../../src/kits/objs

CC	= g++ -m32
LL	= g++ -m32

include ../../makefile.rules

all: $(EXE)

$(EXE): UnitTester.o UnitTester.cpp Makefile
	$(LL) UnitTester.o -L$(COSMOELIBDIR) -lbetest -lcosmoe -L../../src/tools/cppunit/objs -lcppunit -o $@

install: $(EXE) $(bindir)
	cp -f $(EXE) $(bindir)

clean:
	-rm $(EXE) $(OBJS) *~

distclean: clean
	-rm -f Makefile

.cpp.o:
	$(CC) $(COPTS) $< -o $@

.PHONY: clean distclean deps doc install uninstall all
