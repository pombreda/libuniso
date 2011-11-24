-include config.mk

ABI_VERSION := 0
SONAME := libuniso.so.$(ABI_VERSION)

progs-y	+= uniso

INCLUDES := uniso.h
TARGETS = libuniso.a $(progs-y) $(shlibs-y) $(lualibs-y)

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

LUA_VERSION ?= 5.1
LUA_LIBDIR ?= $(prefix)/lib/lua/$(LUA_VERSION)
LUA_SHAREDIR ?= $(prefix)/share/lua/$(LUA_VERSION)

install-progs-y := $(INSTALLDIR) $(DESTDIR)$(bindir) && \
		   $(INSTALL) $(progs-y) $(DESTDIR)$(bindir)
install-libs-y := $(INSTALLDIR) $(DESTDIR)$(libdir) && \
		  $(INSTALL) libuniso.a $(DESTDIR)$(libdir)

OBJS_libuniso.a = libuniso.o

OBJS_$(SONAME) = $(OBJS_libuniso.a)
LDFLAGS_$(SONAME) = -shared -Wl,-soname,$(SONAME)

OBJS_uniso := uniso.o
LIBS_uniso = $(UNISO_LIBS)

CFLAGS_lua-uniso.o = $(LUA_CFLAGS)
OBJS_uniso.so = lua-uniso.o
LIBS_uniso.so = $(UNISO_LIBS) $(LUA_LIBS)
LDFLAGS_uniso.so = -shared

ifneq ($(ENABLE_SHARED),)
shlibs-y += $(SONAME) libuniso.so
install-shlibs-y := $(INSTALLDIR) $(DESTDIR)$(libdir) && \
		    $(INSTALL) $(SONAME) $(DESTDIR)$(libdir) && \
		    ln -sf $(SONAME) $(DESTDIR)$(libdir)/libuniso.so
UNISO_LIBS = -luniso
else
UNISO_LIBS = libuniso.a
endif

ifneq ($(ENABLE_LUA),)
lualibs-y += uniso.so
install-lualibs-y := $(INSTALLDIR) $(DESTDIR)$(LUA_LIBDIR) && \
		     $(INSTALL) uniso.so $(DESTDIR)$(LUA_LIBDIR)
endif

all:	$(TARGETS)

libuniso.so:
	ln -s $(SONAME) $@

$(SONAME): $(OBJS_$(SONAME)) libuniso.so
uniso: $(shlib-y)
uniso.so:  $(OBJS_uniso.so) $(shlibs-y) libuniso.a

libuniso.a: $(OBJS_libuniso.a)
	$(AR) rcs $@ $^
	
%.o: %.c
	$(CC) $(CFLAGS) $(CFLAGS_$@) -c $^

uniso: $(OBJS_uniso) $(shlibs-y) libuniso.a

uniso $(SONAME) uniso.so: 
	$(CC) $(LDFLAGS) $(LDFLAGS_$@) -o $@ $(OBJS_$@) $(LIBS_$@)

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
	$(install-lualibs-y)

