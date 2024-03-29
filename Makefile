BLOCKBAR_SRCS=blockbar.c config.c exec.c modules.c render.c socket.c task.c util.c window-common.c
BLOCKBAR_X11_SRCS=tray.c window.c
BLOCKBAR_WL_SRCS=wl.c
WL_PROTOCOL_DIR=$(shell pkgconf --variable=pkgdatadir wayland-protocols)
WL_PROTOCOL=stable/xdg-shell/xdg-shell.xml wlr-layer-shell-unstable-v1.xml
BBC_SRCS=bbc.c
BLOCKBAR_OBJS=$(BLOCKBAR_SRCS:.c=.o)
WL_HEADERS=$(addprefix protocol/,$(WL_PROTOCOL:.xml=-client-protocol.h))
BLOCKBAR_WL_OBJS=$(addprefix protocol/,$(WL_PROTOCOL:.xml=-protocol.o))
BBC_OBJS=$(BBC_SRCS:.c=.o)
MODULES=text subblocks
MODULEDIRS=$(addprefix modules/,$(MODULES))

VPATH=src

ifeq ($(WAYLAND),1)
BLOCKBAR_SRCS+=$(BLOCKBAR_WL_SRCS)
else
BLOCKBAR_SRCS+=$(BLOCKBAR_X11_SRCS)
endif

DEPS=$(addprefix $(VPATH)/,$(BLOCKBAR_SRCS:.c=.d) $(BBC_SRCS:.c=.d))

PREFIX?=/usr/local

CFLAGS+=-std=gnu99 -Wall -Wextra -D_WITH_DPRINTF
CFLAGS+=-Iinclude/blockbar
CFLAGS+=$(shell pkgconf --cflags cairo)

LDFLAGS+=-rdynamic
LDLIBS+=$(shell pkgconf --libs cairo)
LDLIBS+=-ldl
LDLIBS+=-lujson

DESTDIR?=
BINDIR?=$(PREFIX)/bin
INCDIR?=$(PREFIX)/include
MANDIR?=$(PREFIX)/share/man
BASHDIR?=$(PREFIX)/share/bash-completion
ZSHDIR?=$(PREFIX)/share/zsh/site-functions
MODDIR?=$(PREFIX)/lib/blockbar/modules

CFLAGS+='-DMODDIRS="$(abspath $(MODDIR))"'

ifeq ($(DEBUG),1)
CFLAGS+=-Og -ggdb
endif
export DEBUG

all: $(DEPS)
src/wl.d: $(WL_HEADERS)

-include $(DEPS)

all: blockbar bbc modules

ifeq ($(WAYLAND),1)
CFLAGS+=-Iprotocol -DWAYLAND
CFLAGS+=$(shell pkgconf --cflags wayland-client)
LDLIBS+=$(shell pkgconf --libs wayland-client)

blockbar: $(WL_HEADERS) $(BLOCKBAR_OBJS) $(BLOCKBAR_WL_OBJS)
else
CFLAGS+=$(shell pkgconf --cflags x11)
CFLAGS+=$(shell pkgconf --cflags xrandr)
LDLIBS+=$(shell pkgconf --libs x11)
LDLIBS+=$(shell pkgconf --libs xrandr)

blockbar: $(BLOCKBAR_OBJS)
endif

bbc: $(BBC_OBJS)

modules:
	$(foreach m,$(MODULEDIRS),$(MAKE) -C $(m) && ) true

%.d: %.c
	$(CC) $(CFLAGS) $< -MM -MT $(@:.d=.o) > $@

protocol/%-protocol.c: $(WL_PROTOCOL_DIR)/%.xml
	mkdir -p $(dir $@)
	wayland-scanner private-code $< $@

protocol/%-client-protocol.h: $(WL_PROTOCOL_DIR)/%.xml
	mkdir -p $(dir $@)
	wayland-scanner client-header $< $@

protocol/%-protocol.c: protocol/%.xml
	wayland-scanner private-code $< $@

protocol/%-client-protocol.h: protocol/%.xml
	wayland-scanner client-header $< $@

install: all
	mkdir -p "$(DESTDIR)$(BINDIR)"
	mkdir -p "$(DESTDIR)$(INCDIR)"
	mkdir -p "$(DESTDIR)$(MANDIR)/man1"
	mkdir -p "$(DESTDIR)$(BASHDIR)"
	mkdir -p "$(DESTDIR)$(ZSHDIR)"
	mkdir -p "$(DESTDIR)$(MODDIR)"
	cp -fp blockbar "$(DESTDIR)$(BINDIR)"
	cp -fp bbc "$(DESTDIR)$(BINDIR)"
	cp -fpr include/blockbar "$(DESTDIR)$(INCDIR)"
	cp -fp doc/blockbar.1 "$(DESTDIR)$(MANDIR)/man1"
	cp -fp autocomplete/bbc.bash "$(DESTDIR)$(BASHDIR)/bbc"
	cp -fp autocomplete/bbc.zsh "$(DESTDIR)$(ZSHDIR)/_bbc"
	cp -fp modules/rebuild_modules.sh "$(DESTDIR)$(MODDIR)"
	$(foreach m,$(MODULES),cp -fp "modules/$(m)/$(m).so" "$(DESTDIR)$(MODDIR)" && ) true

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/blockbar"
	rm -f "$(DESTDIR)$(BINDIR)/bbc"
	rm -fr "$(DESTDIR)$(INCDIR)/blockbar"
	rm -f "$(DESTDIR)$(MANDIR)/man1/blockbar.1"
	rm -f "$(DESTDIR)$(BASHDIR)/bbc"
	rm -f "$(DESTDIR)$(ZSHDIR)/_bbc"
	rm -rf "$(DESTDIR)$(MODDIR)"

clean:
	rm -f blockbar bbc
	rm -f $(addprefix $(VPATH)/,$(BLOCKBAR_OBJS) $(BBC_OBJS)) $(DEPS)
	$(foreach m,$(MODULEDIRS),$(MAKE) clean -C $(m) && ) true
	rm -f $(addprefix protocol/,$(WL_PROTOCOL:.xml=-protocol.c))
	rm -f $(addprefix protocol/,$(WL_PROTOCOL:.xml=-client-protocol.h))
	rm -f $(addprefix protocol/,$(WL_PROTOCOL:.xml=-protocol.o))

.PHONY: all modules install uninstall clean
