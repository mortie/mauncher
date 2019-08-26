CFLAGS += -D_GNU_SOURCE -O2 -Igtk-layer-shell/usr/include $(shell pkg-config --cflags gtk+-3.0 wayland-client)
LDFLAGS += -Wl,--no-as-needed -lpthread $(shell pkg-config --libs gtk+-3.0 wayland-client)
PREFIX ?= /usr/local

all: mauncher mauncher-launcher

mauncher mauncher.c: gtk-layer-shell/usr/lib/libgtk-layer-shell.a
mauncher: mauncher.o sysutil.o
mauncher.c: sysutil.h

mauncher-launcher: mauncher-launcher.o sysutil.o
mauncher-launcher.c: sysutil.c

gtk-layer-shell/usr/lib/libgtk-layer-shell.a:
	[ -f gtk-layer-shell/.git ] || git submodule update --init gtk-layer-shell
	(cd gtk-layer-shell && \
		meson build --prefix /usr -Ddefault_library=static && \
		ninja -C build && \
		DESTDIR=`pwd` ninja -C build install)

.PHONY: clean
clean:
	rm -f mauncher mauncher-launcher *.o

.PHONY: cleanall
cleanall: clean
	rm -rf gtk-layer-shell

.PHONY: install
install: mauncher mauncher-launcher
	mkdir -p $(DESTDIR)$(PREFIX)/bin
	cp -f $^ $(DESTDIR)$(PREFIX)/bin
	chmod 755 $(DESTDIR)$(PREFIX)/bin/mauncher
	chmod 755 $(DESTDIR)$(PREFIX)/bin/mauncher-launcher

.PHONY: uninstall
uninstall:
	rm -f $(DESTDIR)$(PREFIX)/bin/mauncher
	rm -f $(DESTDIR)$(PREFIX)/bin/mauncher-launcher
