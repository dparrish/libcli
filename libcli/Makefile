MAJOR=1
MINOR=0
REVISION=0
FILENAME=libcli.so.$(MAJOR).$(MINOR).$(REVISION)


INSTALLPATH=/usr/local/lib

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

install: $(FILENAME)
	install -D $(FILENAME) -g root -m 0755 /usr/local/lib/$(FILENAME)
