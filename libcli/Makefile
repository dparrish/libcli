MAJOR=1
MINOR=0
REVISION=1
FILENAME=libcli.so.$(MAJOR).$(MINOR).$(REVISION)

PREFIX?=/usr/local

CC=gcc
CFLAGS+=-g -O3 -Wall -funroll-loops
LDFLAGS+=-g -shared -nostartfiles -Wl,-soname,libcli.so
LIBPATH+= -L.

all:	libcli.so clitest

libcli.so: libcli.o
	$(CC) -o $(FILENAME) $^ $(LDFLAGS) $(LIBS)
	ln -s $(FILENAME) libcli.so

%.o: %.c
	$(CC) -fPIC -o $@ -c $^ $(CFLAGS) $(INCLUDE_PATH)

clitest: clitest.o
	$(CC) $(CFLAGS) -o $@ $^ -L. -lcli

clean:
	rm -f *.o clitest

install:
	install -D -g root -m 0755 $(FILENAME) $(PREFIX)/lib/$(FILENAME)
	install -D -g root -m 0644 libcli.h $(PREFIX)/include/libcli.h

rpm:
	mkdir libcli-$(MAJOR).$(MINOR).$(REVISION)
	cp -R *.c *.h Makefile Doc README *.spec libcli-$(MAJOR).$(MINOR).$(REVISION)
	tar zcvf libcli-$(MAJOR).$(MINOR).$(REVISION).tar.gz --exclude CVS --exclude *.tar.gz libcli-$(MAJOR).$(MINOR).$(REVISION)
	rm -rf libcli-$(MAJOR).$(MINOR).$(REVISION)
	rpm -ta libcli-$(MAJOR).$(MINOR).$(REVISION).tar.gz --clean
