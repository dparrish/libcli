# Build dynamic library by default
DYNAMIC_LIB ?= 1
# Build static library by default
STATIC_LIB ?= 1
# Run tests by default
TESTS ?= 1

UNAME = $(shell sh -c 'uname -s 2>/dev/null || echo not')
DESTDIR =
PREFIX = /usr/local

MAJOR = 1
MINOR = 10
REVISION = 4
LIB = libcli.so
LIB_STATIC = libcli.a

CC = gcc
AR = ar
ARFLAGS = rcs
DEBUG = -g
OPTIM = -O3
override CFLAGS += $(DEBUG) $(OPTIM) -Wall -std=c99 -pedantic -Wformat-security -Wno-format-zero-length -Werror -Wwrite-strings -Wformat -fdiagnostics-show-option -Wextra -Wsign-compare -Wcast-align -Wno-unused-parameter
override LDFLAGS += -shared
override LIBPATH += -L.

ifeq ($(UNAME),Darwin)
override LDFLAGS += -Wl,-install_name,$(LIB).$(MAJOR).$(MINOR)
else
override LDFLAGS += -Wl,-soname,$(LIB).$(MAJOR).$(MINOR)
LIBS = -lcrypt
endif

ifeq (1,$(DYNAMIC_LIB))
TARGET_LIBS += $(LIB)
endif
ifeq (1,$(STATIC_LIB))
TARGET_LIBS += $(LIB_STATIC)
endif

all: $(TARGET_LIBS) $(if $(filter 1,$(TESTS)),clitest)

$(LIB): libcli.o
	$(CC) -o $(LIB).$(MAJOR).$(MINOR).$(REVISION) $^ $(LDFLAGS) $(LIBS)
	-rm -f $(LIB) $(LIB).$(MAJOR).$(MINOR)
	ln -s $(LIB).$(MAJOR).$(MINOR).$(REVISION) $(LIB).$(MAJOR).$(MINOR)
	ln -s $(LIB).$(MAJOR).$(MINOR) $(LIB)

$(LIB_STATIC): libcli.o
	$(AR) $(ARFLAGS) $@ $^

%.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -fPIC -o $@ -c $<

libcli.o: libcli.h

clitest: clitest.o $(LIB)
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< -L. -lcli

clitest.exe: clitest.c libcli.o
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $< libcli.o -lws2_32

clean:
	rm -f *.o $(LIB)* $(LIB_STATIC) clitest libcli-$(MAJOR).$(MINOR).$(REVISION).tar.gz

install: $(TARGET_LIBS)
	install -d $(DESTDIR)$(PREFIX)/include $(DESTDIR)$(PREFIX)/lib
	install -m 0644 libcli.h $(DESTDIR)$(PREFIX)/include
  ifeq (1,$(STATIC_LIB))
	install -m 0644 $(LIB_STATIC) $(DESTDIR)$(PREFIX)/lib
  endif
  ifeq (1,$(DYNAMIC_LIB))
	install -m 0755 $(LIB).$(MAJOR).$(MINOR).$(REVISION) $(DESTDIR)$(PREFIX)/lib
	cd $(DESTDIR)$(PREFIX)/lib && \
	    ln -fs $(LIB).$(MAJOR).$(MINOR).$(REVISION) $(LIB).$(MAJOR).$(MINOR) && \
	    ln -fs $(LIB).$(MAJOR).$(MINOR) $(LIB)
  endif

rpmprep:
	rm -rf libcli-$(MAJOR).$(MINOR).$(REVISION)
	mkdir libcli-$(MAJOR).$(MINOR).$(REVISION)
	cp -R libcli.{c,h} libcli.spec clitest.c Makefile COPYING README.md doc libcli-$(MAJOR).$(MINOR).$(REVISION)
	tar zcvf libcli-$(MAJOR).$(MINOR).$(REVISION).tar.gz --exclude CVS --exclude *.tar.gz libcli-$(MAJOR).$(MINOR).$(REVISION)
	rm -rf libcli-$(MAJOR).$(MINOR).$(REVISION)

rpm: rpmprep
	rpmbuild -ta libcli-$(MAJOR).$(MINOR).$(REVISION).tar.gz --define "debug_package %{nil}" --clean

lint:
	clang-tidy -quiet -warnings-as-errors *.c *.h

