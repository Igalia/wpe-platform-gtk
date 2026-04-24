# WPE Platform GTK

A GTK implementation of WPE platform.

## Description

`WPEPlatform` is a modern platform API designed to replace the legacy combination of `libwpe` and `WPEBackend`s. It simplifies the development of embedded web-based applications by providing a unified, `GObject`-based approach to platform abstraction, rendering, and input handling.

Out of the box, WPE provides built-in platform implementations for DRM, Wayland, and headless environments.

`WPEPlatformGTK` is an external implementation that integrates with the GTK toolkit and allows WebKit to be easily embedded into any GTK application.

## Dependencies

- wpe-platform-2.0 (>= 2.51.3)
- gmodule-2.0 (>= 2.70.0)
- gtk4 (>= 4.16.0)
- epoxy (>= 1.4)

## Building and Installation

This project uses the [Meson](https://mesonbuild.com) build system and follows standard workflow:

```sh
meson setup builddir
meson compile -C builddir
meson install -C builddir
```

After installation, run `ldconfig` to update the dynamic linker cache.

## License

This project is licensed under the terms of the MIT license.
