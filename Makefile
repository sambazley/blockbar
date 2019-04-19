BLOCKBAR_SRCS=blockbar.c config.c exec.c modules.c render.c socket.c tray.c util.c window.c
BBC_SRCS=bbc.c
BLOCKBAR_OBJS=$(BLOCKBAR_SRCS:.c=.o)
BBC_OBJS=$(BBC_SRCS:.c=.o)
MODULES=legacy subblocks
MODULEDIRS=$(addprefix modules/,$(MODULES))
DOCS=$(addprefix doc/,blockbar.1.gz)

VPATH=src

DEPS=$(addprefix $(VPATH)/,$(BLOCKBAR_SRCS:.c=.d) $(BBC_SRCS:.c=.d))

PREFIX?=/usr/local

CFLAGS+=-std=gnu99 -Wall -Wextra -D_WITH_DPRINTF
CFLAGS+=-Iinclude/blockbar
CFLAGS+=$(shell pkgconf --cflags cairo)
CFLAGS+=$(shell pkgconf --cflags x11)
CFLAGS+=$(shell pkgconf --cflags xrandr)

LDFLAGS+=-rdynamic
LDLIBS+=$(shell pkgconf --libs cairo)
LDLIBS+=$(shell pkgconf --libs x11)
LDLIBS+=$(shell pkgconf --libs xrandr)
LDLIBS+=-ldl
LDLIBS+=-lujson

DESTDIR?=
BINDIR?=$(DESTDIR)$(PREFIX)/bin
INCDIR?=$(DESTDIR)$(PREFIX)/include
MANDIR?=$(DESTDIR)$(PREFIX)/share/man
BASHDIR?=$(DESTDIR)$(PREFIX)/share/bash-completion
ZSHDIR?=$(DESTDIR)$(PREFIX)/share/zsh/site-functions
MODDIR?=$(DESTDIR)$(PREFIX)/lib/blockbar

ifeq ($(DEBUG),1)
CFLAGS+=-Og -ggdb
endif
export DEBUG

all: $(DEPS)
-include $(DEPS)

all: blockbar bbc modules $(DOCS)

blockbar: $(BLOCKBAR_OBJS)

bbc: $(BBC_OBJS)

modules:
	$(foreach m,$(MODULEDIRS),$(MAKE) -C $(m) && ) true

doc/%.gz: doc/%
	gzip -f < $< > $@

%.d: %.c
	$(CC) $(CFLAGS) $< -MM -MT $(@:.d=.o) > $@

install: all
	mkdir -p "$(BINDIR)"
	mkdir -p "$(INCDIR)"
	mkdir -p "$(MANDIR)/man1"
	mkdir -p "$(BASHDIR)"
	mkdir -p "$(ZSHDIR)"
	mkdir -p "$(MODDIR)"
	cp -fp blockbar "$(BINDIR)"
	cp -fp bbc "$(BINDIR)"
	cp -fpr include/blockbar "$(INCDIR)"
	cp -fp doc/blockbar.1.gz "$(MANDIR)/man1"
	cp -fp autocomplete/bbc.bash "$(BASHDIR)/bbc"
	cp -fp autocomplete/bbc.zsh "$(ZSHDIR)/_bbc"
	$(foreach m,$(MODULES),cp -fp "modules/$(m)/$(m).so" $(MODDIR) && ) true

uninstall:
	rm -f "$(BINDIR)/blockbar"
	rm -f "$(BINDIR)/bbc"
	rm -f "$(MANDIR)/man1/blockbar.1.gz"
	rm -f "$(BASHDIR)/bbc"
	rm -f "$(ZSHDIR)/_bbc"
	rm -rf "$(MODDIR)"

clean:
	rm -f blockbar bbc
	rm -f $(addprefix $(VPATH)/,$(BLOCKBAR_OBJS) $(BBC_OBJS)) $(DEPS)
	rm -f $(DOCS)
	$(foreach m,$(MODULEDIRS),$(MAKE) clean -C $(m) && ) true

.PHONY: all modules install uninstall clean
