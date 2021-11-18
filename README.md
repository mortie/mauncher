# Mauncher

Mauncher is a GTK-based alternative to dmenu for Wayland which supports display
scaling.

![Screenshot](https://raw.githubusercontent.com/mortie/mauncher/master/screenshot.png)

## Installation

### From Package

The [mauncher-git](https://aur.archlinux.org/packages/mauncher-git/) package is
available for Arch Linux.

### Compiling From Source

Run `meson build` to configure, `ninja -C build install` to install, and `ninja
-C build uninstall` to uninstall. Depending on your configuration, you may need
to elevate your perimssions to install.

Dependencies:

* meson
* ninja
* git
* gtk3
* gobject-introspection

## Usage

Mauncher comes with a launcher called `mauncher-launcher`,
so running `mauncher-launcher` will start a launcher which lists desktop files,
supports math (through a python interpreter), running shell commands by
prefixing the string with a `$`, etc.

Otherwise, mauncher works like dmenu; give it a newline-separated list of strings on
stdin, the user selects an item, and that item is printed to stdout.

Just as you would use `dmenu_path | dmenu | sh` to use dmenu as a launcher, you
can use `dmenu_path | mauncher | sh` to use mauncher as a launcher.
