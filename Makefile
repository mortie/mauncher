CFLAGS += -D_GNU_SOURCE -O2 -Igtk-layer-shell/usr/include $(shell pkg-config --cflags gtk+-3.0 wayland-client)
LDFLAGS += -lpthread $(shell pkg-config --libs gtk+-3.0 wayland-client)
PREFIX ?= /usr/local

all: mauncher
mauncher mauncher.c: gtk-layer-shell/usr/lib/libgtk-layer-shell.a

gtk-layer-shell/usr/lib/libgtk-layer-shell.a:
	[ -f gtk-layer-shell/.git ] || git submodule update --init gtk-layer-shell
	(cd gtk-layer-shell && \
		LDFLAGS= CFLAGS= CPPFLAGS= meson build --prefix /usr -Ddefault_library=static && \
		ninja -C build && \
		DESTDIR=`pwd` ninja -C build install)

.PHONY: clean
clean:
	rm -f mauncher

.PHONY: cleanall
cleanall: clean
	rm -rf gtk-layer-shell

.PHONY: install
install: mauncher
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f $^ $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/mauncher

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/mauncher
