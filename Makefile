
ABI_VERSION := 0
SONAME := libuniso.so.$(ABI_VERSION)
OBJS := libuniso.o


CFLAGS ?= -g -Wall 
CFLAGS += -fPIC

all:	libuniso.so

$(SONAME): $(OBJS)
	$(CC) -shared -Wl,-soname,$(SONAME) -o $@

libuniso.so: $(SONAME)
	ln -s $< $@

%.o: %.c
	$(CC) $(CFLAGS) -c $^

clean:
	rm -f libuniso.so libuniso.so.$(ABI_VERSION) *.o

