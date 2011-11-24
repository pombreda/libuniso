
ABI_VERSION := 0
SONAME := libuniso.so.$(ABI_VERSION)

TARGETS := $(SONAME) libuniso.so uniso

CFLAGS ?= -g -Wall 
CFLAGS += -fPIC
CFLAGS += -I.

$(SONAME)_OBJS = libuniso.o
$(SONAME)_LDFLAGS = -shared -Wl,-soname,$(SONAME)

uniso_OBJS := uniso.o
uniso_LIBS := -luniso
uniso_LDFLAGS += -L.

all:	$(TARGETS)

$(SONAME): $($(SONAME)_OBJS)
libuniso.so: $(SONAME)
	ln -s $< $@

%.o: %.c
	$(CC) $(CFLAGS) $($@_CFLAGS) -c $^

uniso: $(uniso_OBJS) libuniso.so

uniso $(SONAME):
	$(CC) $(LDFLAGS) $($@_LDFLAGS) -o $@ $($@_OBJS) $($@_LIBS)

clean:
	rm -f $(TARGETS) *.o


