-include config.mk

ABI_VERSION := 0
SONAME := libuniso.so.$(ABI_VERSION)

progs-y	+= uniso

INCLUDES := uniso.h
TARGETS := libuniso.a $(progs-y)

CFLAGS ?= -g -Wall -Werror
CFLAGS += -fPIC
CFLAGS += -I.

LDFLAGS += -L.

prefix ?= /usr
libdir = $(prefix)/lib
bindir = $(prefix)/bin
includedir= $(prefix)/include

INSTALLDIR := install -d
INSTALL := install
PKG_CONFIG ?= pkg-config

LUA_LIBS ?= $(shell $(PKG_CONFIG) --libs lua)
LUA_CFLAGS ?= $(shell $(PKG_CONFIG) --cflags lua)

install-progs-y := $(INSTALLDIR) $(DESTDIR)$(bindir) && \
		   $(INSTALL) $(progs-y) $(DESTDIR)$(bindir)
install-libs-y := $(INSTALLDIR) $(DESTDIR)$(libdir) && \
		  $(INSTALL) libuniso.a $(DESTDIR)$(libdir)

libuniso.a_OBJS = libuniso.o

$(SONAME)_OBJS = $(libuniso.a_OBJS)
$(SONAME)_LDFLAGS = -shared -Wl,-soname,$(SONAME)

uniso_OBJS := uniso.o
uniso_LIBS = $(LIBS_UNISO)

lua-uniso.o_CFLAGS = $(LUA_CFLAGS)
uniso.so_OBJS = lua-uniso.o
uniso.so_LIBS = $(LIBS_UNISO) $(LUA_LIBS)
uniso.so_LDFLAGS = -shared

ifneq ($(ENABLE_SHARED),)
shlibs-y += $(SONAME) libuniso.so
install-shlibs-y := $(INSTALLDIR) $(DESTDIR)$(libdir) && \
		    $(INSTALL) $(SONAME) $(DESTDIR)$(libdir) && \
		    ln -sf $(SONAME) $(DESTDIR)$(libdir)/libuniso.so
LIBS_UNISO = -luniso
else
LIBS_UNISO = libuniso.a
endif
TARGETS += $(shlibs-y)

ifneq ($(ENABLE_LUA),)
lualibs-y += uniso.so
endif
TARGETS += $(lualibs-y)

all:	$(TARGETS)

libuniso.so:
	ln -s $(SONAME) $@

$(SONAME): $($(SONAME)_OBJS) libuniso.so
uniso: $(shlib-y)
uniso.so:  $(uniso.so_OBJS) $(shlibs-y) libuniso.a

libuniso.a: $(libuniso.a_OBJS)
	$(AR) rcs $@ $^
	
%.o: %.c
	$(CC) $(CFLAGS) $($@_CFLAGS) -c $^

uniso: $(uniso_OBJS) $(shlibs-y) libuniso.a

uniso $(SONAME) uniso.so: 
	$(CC) $(LDFLAGS) $($@_LDFLAGS) -o $@ $($@_OBJS) $($@_LIBS)

clean:
	rm -f $(TARGETS) *.o *.a *.so *.so.$(ABI_VERSION)

shared: $(SONAME)

lua: uniso.so

install: $(TARGETS) $(INCLUDES)
	$(INSTALLDIR) $(DESTDIR)$(includedir)
	$(INSTALL) $(INCLUDES) $(DESTDIR)$(includedir)
	$(install-progs-y)
	$(install-libs-y)
	$(install-shlibs-y)

