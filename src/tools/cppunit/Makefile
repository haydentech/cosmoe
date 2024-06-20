prefix=/usr/local
libdir=${prefix}/lib

CFLAGS=-c -fPIC -g -Wno-multichar  -I. -I../../../headers/tools/cppunit `cat ../../../cosmoe.specs | tr '\n' ' '`
CPPFLAGS=$(CFLAGS)
CC=gcc -m32

OBJS =	BTestCase.o \
		BTestSuite.o \
		SemaphoreSyncObject.o \
		TestApp.o \
		TestListener.o \
		TestShell.o \
		TestUtils.o \
		ThreadedTestCase.o

OBJDIR  := objs

LIBNAME = libbetest.so

COSMOELIBDIR = ../../../src/kits/objs


include ../../../makefile.rules

all :   $(OBJDIR) $(OBJDIR)/$(LIBNAME)

$(OBJDIR):
	mkdir $(OBJDIR)

$(OBJDIR)/$(LIBNAME): $(OBJS)
	#ar rcs $(OBJDIR)/$(LIBNAME) $(OBJS)
	$(CC) -shared $(OBJS) -L$(COSMOELIBDIR) -lcosmoe -o $(OBJDIR)/$(LIBNAME)

$(libdir):
	mkdir -p $(libdir)

install: $(OBJDIR)/$(LIBNAME) $(libdir)
	cp -f $< $(libdir)/$(LIBNAME)

deps:

clean:
	rm -rf $(OBJDIR)

distclean: clean
	rm Makefile
