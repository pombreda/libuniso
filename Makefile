-include config.mk

ABI_VERSION := 0
SONAME := libuniso.so.$(ABI_VERSION)

progs-y	+= uniso

INCLUDES := uniso.h
TARGETS := libuniso.a $(progs-y)

CFLAGS ?= -g -Wall -Werror
CFLAGS += -fPIC
CFLAGS += -I.


prefix ?= /usr
libdir = $(prefix)/lib
bindir = $(prefix)/bin
includedir= $(prefix)/include

INSTALLDIR := install -d
INSTALL := install

install-progs-y := $(INSTALLDIR) $(DESTDIR)$(bindir) && \
		   $(INSTALL) $(progs-y) $(DESTDIR)$(bindir)
install-libs-y := $(INSTALLDIR) $(DESTDIR)$(libdir) && \
		  $(INSTALL) libuniso.a $(DESTDIR)$(libdir)

libuniso.a_OBJS = libuniso.o

$(SONAME)_OBJS = $(libuniso.a_OBJS)
$(SONAME)_LDFLAGS = -shared -Wl,-soname,$(SONAME)

uniso_OBJS := uniso.o
uniso_LDFLAGS += -L.

all:	$(TARGETS)

ifneq ($(ENABLE_SHARED),)
shlibs-y += $(SONAME) libuniso.so
install-shlibs-y := $(INSTALLDIR) $(DESTDIR)$(libdir) && \
		    $(INSTALL) $(SONAME) $(DESTDIR)$(libdir) && \
		    ln -sf $(SONAME) $(DESTDIR)$(libdir)/libuniso.so
uniso_LIBS := -luniso
else
uniso_LIBS := libuniso.a
endif
TARGETS += $(shlibs-y)

$(SONAME): $($(SONAME)_OBJS)
libuniso.so: $(SONAME)
	ln -s $< $@

uniso: $(shlib-y)

libuniso.a: $(libuniso.a_OBJS)
	$(AR) rcs $@ $^
	
%.o: %.c
	$(CC) $(CFLAGS) $($@_CFLAGS) -c $^

uniso: $(uniso_OBJS) $(shlibs-y)

uniso $(SONAME):
	$(CC) $(LDFLAGS) $($@_LDFLAGS) -o $@ $($@_OBJS) $($@_LIBS)

clean:
	rm -f $(TARGETS) *.o *.a *.so *.so.$(ABI_VERSION)

shared: $(SONAME)

install: $(TARGETS) $(INCLUDES)
	$(INSTALLDIR) $(DESTDIR)$(includedir)
	$(INSTALL) $(INCLUDES) $(DESTDIR)$(includedir)
	$(install-progs-y)
	$(install-libs-y)
	$(install-shlibs-y)

