CFLAGS+=-fPIC -std=c99 -I../../include -ggdb -Og
CFLAGS+=$(shell pkgconf --cflags cairo)
CFLAGS+=$(shell pkgconf --cflags pangocairo)

LDFLAGS+=-shared
LDFLAGS+=$(shell pkgconf --libs cairo)
LDFLAGS+=$(shell pkgconf --libs pangocairo)

.PHONY: all clean

all: subblocks.so

subblocks.so: main.o
	$(CC) $^ -o $@ $(LDFLAGS)

main.o: main.c

clean:
	rm *.o *.so
