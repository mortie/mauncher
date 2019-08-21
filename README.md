# Mauncher

Mauncher is a GTK-based alternative to dmenu for Wayland which supports display
scaling.

![Screenshot](https://raw.githubusercontent.com/mortie/mauncher/master/screenshot.png)

## Installation

### From Package

The [mauncher-git](https://aur.archlinux.org/packages/mauncher-git/) package is
available for Arch Linux.

### Compiling From Source

Run `make` to compile, `sudo make install` to install, and `sudo make uninstall`
to uninstall.

Dependencies:

* meson
* git
* gtk3
* gobject-introspection

## Usage

Mauncher works like dmenu; give it a newline-separated list of strings on
stdin, the user selects an item, and that item is printed to stdout.

Just as you would use `dmenu_path | dmenu | sh` to use dmenu as a launcher, you
can use `dmenu_path | mauncher | sh` to use mauncher as a launcher.

You should check out [mmenu](https://github.com/mortie/mmenu) to add a
python-based calculator. Since Mauncher works like dmenu, it works jus fine
with `mmenu mauncher`.
