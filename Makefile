DESTDIR =
PREFIX = /usr/local

MAJOR = 2
MINOR = 0
REVISION = 0
LIB = libcli.so

CC = gcc
DEBUG = -g
OPTIM = -O3
LIBS = -lcrypt

# Enable threading support
OPTS += -DLIBCLI_THREADED
#OPTS += -DSTRINGBUFFER_DEBUG
LIBS += -lpthread

CFLAGS += $(DEBUG) $(OPTIM) -Wall -Wformat-security -Wno-format-zero-length $(OPTS)
LDFLAGS += -shared -Wl,-soname,$(LIB).$(MAJOR).$(MINOR)
LIBPATH += -L.

all: $(LIB) clitest

$(LIB): libcli.o stringbuffer.o internal_commands.o
	$(CC) -o $(LIB).$(MAJOR).$(MINOR).$(REVISION) $^ $(LDFLAGS) $(LIBS)
	-rm -f $(LIB) $(LIB).$(MAJOR).$(MINOR)
	ln -s $(LIB).$(MAJOR).$(MINOR).$(REVISION) $(LIB).$(MAJOR).$(MINOR)
	ln -s $(LIB).$(MAJOR).$(MINOR) $(LIB)

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -o $@ -c $<

libcli.o: libcli.h

clitest: clitest.o stringbuffer.o $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< -L. -lcli

clitest.exe: clitest.c libcli.o
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< libcli.o -lws2_32

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
