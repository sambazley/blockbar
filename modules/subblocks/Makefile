CFLAGS+=-fPIC -std=c99 -I../../include -Wall -Wextra
CFLAGS+=$(shell pkgconf --cflags cairo)
CFLAGS+=$(shell pkgconf --cflags pangocairo)

LDFLAGS+=-shared
LDFLAGS+=$(shell pkgconf --libs cairo)
LDFLAGS+=$(shell pkgconf --libs pangocairo)

.PHONY: all clean

ifeq ($(DEBUG),1)
CFLAGS+=-Og -ggdb
endif

all: subblocks.so

subblocks.so: main.o
	$(CC) $^ -o $@ $(LDFLAGS)

clean:
	rm -f *.o *.so
