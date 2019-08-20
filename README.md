# Mauncher

Mauncher is a GTK-based alternative to dmenu for Wayland which supports display
scaling.

![Screenshot](https://raw.githubusercontent.com/mortie/mauncher/master/screenshot.png)

## Usage

Mauncher works like dmenu; give it a newline-separated list of strings on
stdin, the user selects an item, and that item is printed to stdout.

Just as you would use `dmenu_path | dmenu | sh` to use dmenu as a launcher, you
can use `dmenu_path | mauncher | sh` to use mauncher as a launcher.

You should check out [mmenu](https://github.com/mortie/mmenu) to add a
python-based calculator. Since Mauncher works like dmenu, it works jus fine
with `mmenu mauncher`.
