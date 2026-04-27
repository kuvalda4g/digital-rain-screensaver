# Digital Rain Screensaver

A small cross-platform screensaver prototype built with `C++`, `GLFW`, and classic `OpenGL`.

The project renders a fullscreen or windowed "digital rain" scene inspired by classic cyberpunk terminal visuals. It uses built-in bitmap glyph sets, so there are no external font or texture assets to ship.

## Status

This repository currently contains the rendering core:
- runs as a regular desktop application on Windows and Linux;
- supports fullscreen and windowed modes;
- is intended to become the base for future platform-specific screensaver wrappers.

It is not yet packaged as a native OS-level screensaver.

## Features

- procedural glyph generation with no external assets;
- multiple glyph modes, including pseudo-katakana, techno symbols, and custom charsets;
- layered rain rendering with separate background, mid, and foreground streams;
- animated streams with different speeds, trail lengths, and flicker;
- fullscreen or resizable windowed launch modes;
- compact codebase that is easy to extend.

## Requirements

- CMake `3.20+`
- a C++20 compiler
- GLFW `3.x`
- OpenGL

## Build

### Linux

```bash
cmake -S . -B build
cmake --build build
```

If GLFW development files are missing, install them first. On Debian/Ubuntu-based systems it is usually:

```bash
sudo apt install build-essential cmake libglfw3-dev mesa-common-dev
```

### Windows

- install GLFW and make it visible to CMake, for example through `vcpkg`;
- generate the project with CMake;
- build it with Visual Studio or Ninja.

## Run

Windowed mode:

```bash
./build/digital-rain-screensaver
```

Fullscreen mode:

```bash
./build/digital-rain-screensaver --fullscreen
```

Enable horizontal sway:

```bash
./build/digital-rain-screensaver --sway
```

Pseudo-katakana glyphs:

```bash
./build/digital-rain-screensaver --glyph-set pseudo-katakana
```

Techno glyphs:

```bash
./build/digital-rain-screensaver --glyph-set techno
```

Custom ASCII charset:

```bash
./build/digital-rain-screensaver --charset "NEURO-01#"
```

Custom window size:

```bash
./build/digital-rain-screensaver --width 1600 --height 900
```

## Controls

- `Esc` or `Q` closes the application;
- left mouse button also closes it.

## Notes

This project is the rendering core only. A real OS-level screensaver still needs platform-specific integration:

- Windows: `.scr` wrapper and support for `/s`, `/c`, and `/p`;
- Linux: integration depends on the target desktop environment, for example `xscreensaver` or a standalone fullscreen launcher.

## Roadmap

- add a Windows `.scr` wrapper;
- add Linux-oriented launcher/integration options;
- improve the visual style with more variation and configuration.
