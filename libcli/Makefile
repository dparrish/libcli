MAJOR=1
MINOR=6
REVISION=2
LIB=libcli.so

PREFIX?=/usr/local

CC=gcc
DEBUG=-g
CFLAGS+=$(DEBUG) -O3 -Wall -funroll-loops
LDFLAGS+=-shared -nostartfiles -Wl,-soname,$(LIB).$(MAJOR).$(MINOR)
LIBPATH+= -L.
LIBS=-lcrypt

all:	$(LIB) clitest

$(LIB): libcli.o
	$(CC) -o $(LIB).$(MAJOR).$(MINOR).$(REVISION) $^ $(LDFLAGS) $(LIBS)
	-rm -f $(LIB) $(LIB).$(MAJOR).$(MINOR)
	ln -s $(LIB).$(MAJOR).$(MINOR).$(REVISION) $(LIB).$(MAJOR).$(MINOR)
	ln -s $(LIB).$(MAJOR).$(MINOR) $(LIB)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -o $@ -c $^

clitest: clitest.o $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $^ -L. -lcli

clean:
	rm -f *.o $(LIB)* clitest

install: $(LIB)
	install -d $(DESTDIR)$(PREFIX)/include $(DESTDIR)$(PREFIX)/lib
	install -m 0644 libcli.h $(DESTDIR)$(PREFIX)/include
	install -m 0755 $(LIB).$(MAJOR).$(MINOR).$(REVISION) $(DESTDIR)$(PREFIX)/lib
	cd $(DESTDIR)$(PREFIX)/lib && \
	    ln -s $(LIB).$(MAJOR).$(MINOR).$(REVISION) $(LIB).$(MAJOR).$(MINOR) && \
	    ln -s $(LIB).$(MAJOR).$(MINOR) $(LIB)

rpm:
	mkdir libcli-$(MAJOR).$(MINOR).$(REVISION)
	cp -R *.c *.h Makefile Doc README *.spec libcli-$(MAJOR).$(MINOR).$(REVISION)
	tar zcvf libcli-$(MAJOR).$(MINOR).$(REVISION).tar.gz --exclude CVS --exclude *.tar.gz libcli-$(MAJOR).$(MINOR).$(REVISION)
	rm -rf libcli-$(MAJOR).$(MINOR).$(REVISION)
	rpm -ta libcli-$(MAJOR).$(MINOR).$(REVISION).tar.gz --clean
