prefix=/usr/local
bindir=${prefix}/bin

COPTS	= `cat ../../../../cosmoe.specs` -g -Wall -Wno-multichar -I../../../../headers/tools/cppunit -c

OBJS	= AppFileInfoTest.o BasicTest.o DirectoryTest.o EntryTest.o FileTest.o FindDirectoryTest.o \
			NodeInfoTest.o NodeTest.o PathTest.o  \
			StatableTest.o StorageKitTestAddon.o SymLinkTest.o \
			TestApp.o 

# QueryTest.o ResourcesTest.o ResourceStringsTest.o MimeSnifferTest.o MimeTypeTest.o VolumeTest.o

LIBNAME = storagekittest.so

COSMOELIBDIR = ../../../../src/kits/objs

CC	= gcc -m32

all: $(LIBNAME)

$(LIBNAME): $(OBJS) Makefile
	$(CC) -shared $(OBJS) -lstdc++ -L$(COSMOELIBDIR) -lcosmoe -lcppunit -L../../../../src/tools/cppunit/objs -lbetest -o $@

install:
	cp -f $(bindir)

clean:
	-rm $(EXE) $(OBJS) *~

distclean: clean
	-rm -f Makefile

.cpp.o:
	$(CC) $(COPTS) $< -o $@

.PHONY: clean distclean deps doc install uninstall all
