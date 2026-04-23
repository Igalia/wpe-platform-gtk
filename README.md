# WPE Platform GTK

A GTK implementation of WPE platform.

## Dependencies

- wpe-platform-2.0 (>= 2.51.3)
- gmodule-2.0 (>= 2.70.0)
- gtk4 (>= 4.16.0)
- epoxy (>= 1.4)

## Building

```sh
meson setup --prefix=/usr builddir
meson compile -C builddir
```

## Installation

```sh
sudo meson install -C builddir
```
