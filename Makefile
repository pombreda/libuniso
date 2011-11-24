
ABI_VERSION := 0
SONAME := libuniso.so.$(ABI_VERSION)
OBJS := libuniso.o

TARGETS := $(SONAME) libuniso.so uniso

CFLAGS ?= -g -Wall 
CFLAGS += -fPIC
CFLAGS += -I.

uniso_OBJS := uniso.o
uniso_LIBS := -luniso
uniso_LDFLAGS += -L.

all:	$(TARGETS)

$(SONAME): $(OBJS)
	$(CC) -shared -Wl,-soname,$(SONAME) $(LDFLAGS) $($@_LDFLAGS) -o $@ $^

libuniso.so: $(SONAME)
	ln -s $< $@

%.o: %.c
	$(CC) $(CFLAGS) $($@_CFLAGS) -c $^

uniso: $(uniso_OBJS) libuniso.so
	$(CC) $(LDFLAGS) -o $@ $($@_OBJS) $($@_LIBS)

clean:
	rm -f $(TARGETS) *.o


