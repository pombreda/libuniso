-include config.mk

ABI_VERSION := 0
SONAME := libuniso.so.$(ABI_VERSION)

TARGETS := libuniso.a uniso

CFLAGS ?= -g -Wall -Werror
CFLAGS += -fPIC
CFLAGS += -I.

libuniso.a_OBJS = libuniso.o

$(SONAME)_OBJS = $(libuniso.a_OBJS)
$(SONAME)_LDFLAGS = -shared -Wl,-soname,$(SONAME)

uniso_OBJS := uniso.o
uniso_LDFLAGS += -L.

all:	$(TARGETS)

ifneq ($(ENABLE_SHARED_LIB),)
shlibs-y += $(SONAME) libuniso.so
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

